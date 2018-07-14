#include "hooks.h"
#include "fw_bridge.h"
#include "../sdk/framework/utils/threading.h"
#include "../sdk/framework/utils/stackstring.h"
#include "../sdk/framework/utils/memutils.h"
#include <atomic>
#include "../sdk/source_csgo/sdk.h"
#include "../sdk/source_shared/eventlistener.h"
#include "engine.h"
#include "resolver.h"

#include "../signatures.h"
#include "../hook_indices.h"
#include "../interfaces.h"

VFuncHook* hookClientMode = nullptr;
VFuncHook* hookPanel = nullptr;

CBaseClient* cl = nullptr;
IClientMode* clientMode = nullptr;
IVEngineClient* engine = nullptr;
IClientEntityList* entityList = nullptr;
CGlobalVarsBase* globalVars = nullptr;
IVModelInfo* mdlInfo = nullptr;
IEngineTrace* engineTrace = nullptr;
ICvar* cvar = nullptr;
CClientState* clientState = nullptr;
CPrediction* prediction = nullptr;
IPanel* panel = nullptr;
ISurface* surface = nullptr;
IViewRender* viewRender = nullptr;
void* weaponDatabase = nullptr;
CClientEffectRegistration** effectsHead = nullptr;
IGameEventManager* gameEvents = nullptr;

CL_RunPredictionFn CL_RunPrediction = nullptr;
Weapon_ShootPositionFn Weapon_ShootPosition = nullptr;
RunSimulationFn RunSimulationFunc = nullptr;
GetWeaponInfoFn GetWeaponInfo = nullptr;
SetAbsFn SetAbsOrigin = nullptr;
SetAbsFn SetAbsAngles = nullptr;
SetAbsFn SetAbsVelocity = nullptr;
SetupBonesFn SetupBones = nullptr;

RandomSeedFn RandomSeed = nullptr;
RandomFloatFn RandomFloat = nullptr;
RandomFloatExpFn RandomFloatExp = nullptr;
RandomIntFn RandomInt = nullptr;
RandomGaussianFloatFn RandomGaussianFloat = nullptr;

EventListener listener({Resolver::ImpactEvent});

static void InitializeOffsets();
static void InitializeHooks();
static void InitializeDynamicHooks();
void Shutdown();
void Unload();

void* __stdcall EntryPoint(void*)
{
	Threading::InitThreads();
#ifndef LOADER_INITIALIZATION
	InitializeOffsets();
	InitializeHooks();
#endif
	InitializeDynamicHooks();
	return nullptr;
}

Semaphore closeSemaphore;
#if defined(__linux__) || defined(__APPLE__)
#define APIENTRY __attribute__((constructor))
static thread_t* tThread = nullptr;

__attribute__((destructor))
void DLClose()
{
	closeSemaphore.Post();
	usleep(100000);
	Shutdown();
}
#else
void* thisModule = nullptr;
#endif
bool shuttingDown = false;

int APIENTRY DllMain(void* hModule, uintptr_t reasonForCall, void* lpReserved)
{
#ifdef _WIN32
	thisModule = hModule;
	if (reasonForCall == DLL_PROCESS_ATTACH)
#endif
		Threading::StartThread(EntryPoint, NULL);
	return 1;
}

void SigOffset(Signature* sig)
{
	sig->result = PatternScan::FindPattern(sig->pattern, sig->module);
}

static void PlatformSpecificOffsets()
{
#ifdef __posix__
	uintptr_t hudUpdate = (*(uintptr_t**)cl)[11];
	globalVars = *(CGlobalVarsBase**)(GetAbsoluteAddress(hudUpdate + LWM(13, 0, 15), 3, 7));
	//uintptr_t activateMouse = (*(uintptr_t**)cl)[15];
	uintptr_t GetLocalPlayer = GetAbsoluteAddress((*(uintptr_t**)engine)[12] + 9, 1, 5);
	clientState = ((CClientState*(*)(int))GetLocalPlayer)(-1);
#else
	globalVars = **(CGlobalVarsBase***)((*(uintptr_t**)(cl))[0] + 0x1B);
#endif
}

static void FindVSTDFunctions()
{
	MHandle handle = Handles::GetModuleHandle(vstdLib);

	RandomSeed = (RandomSeedFn)paddr(handle, ST("RandomSeed"));
	RandomFloat = (RandomFloatFn)paddr(handle, ST("RandomFloat"));
	RandomFloatExp = (RandomFloatExpFn)paddr(handle, ST("RandomFloatExp"));
	RandomInt = (RandomIntFn)paddr(handle, ST("RandomInt"));
	RandomGaussianFloat = (RandomGaussianFloatFn)paddr(handle, ST("RandomGaussianFloat"));
}

static void InitializeOffsets()
{
	for (size_t i = 0; i < sizeof(signatures) / (sizeof((signatures)[0])); i++)
		Threading::QueueJobRef(SigOffset, signatures + i);

	FindVSTDFunctions();
	FindAllInterfaces(interfaceList, sizeof(interfaceList)/sizeof((interfaceList)[0]));
	PlatformSpecificOffsets();
	SourceNetvars::Initialize(cl);
	Threading::FinishQueue();
}

static void InitializeHooks()
{
	//We have to specify the minSize since vtables on MacOS act strangely with one or two functions being a null pointer
	hookClientMode = new VFuncHook(clientMode, false, 25);
#ifdef PT_VISUALS
	hookPanel = new VFuncHook(panel, false, 5);
#endif

	for (size_t i = 0; i < sizeof(hookIds) / sizeof((hookIds)[0]); i++)
		hookIds[i].hook->Hook(hookIds[i].index, hookIds[i].function);
}

static void InitializeDynamicHooks()
{
	CSGOHooks::entityHooks = new std::unordered_map<C_BasePlayer*, VFuncHook*>();
	EffectsHook::HookAll(effectHooks, effectsCount, *effectsHead);
	listener.Initialize(ST("bullet_impact"));

#ifdef DEBUG
	cvar->ConsoleDPrintf("Effect list:\n");
	Color col = Color(0, 255, 0, 255);
    for (auto head = *effectsHead; head; head = head->next)
		cvar->ConsoleColorPrintf(col, "%s\n", head->effectName);
#endif
}

void Shutdown()
{

	Threading::EndThreads();

	if (hookClientMode) {
		delete hookClientMode;
		hookClientMode = nullptr;
	}

	if (hookPanel) {
		delete hookPanel;
		hookPanel = nullptr;
	}

	if (CSGOHooks::entityHooks) {
		for (auto& i : *CSGOHooks::entityHooks)
			delete i.second;
		delete CSGOHooks::entityHooks;
		CSGOHooks::entityHooks = nullptr;
	}

	EffectsHook::UnhookAll(effectHooks, effectsCount, *effectsHead);

	Engine::Shutdown();
}

void* __stdcall UnloadThread(thread_t* thisThread)
{
	Shutdown();

#if defined(__linux__) || defined(__APPLE__)
	tThread = thisThread;
	MHandle handle = Handles::GetPtrModuleHandle((void*)DLClose);
	dlclose(handle);
	thread_t ctrd;
	int count = 0;
	while (closeSemaphore.TimedWait(50)) {
		if (count++) {
			void* ret;
			pthread_join(ctrd, &ret);
		}
		ctrd = Threading::StartThread((threadFn)dlclose, handle);
	}
	pthread_exit(0);
#else
	Sleep(50);
	FreeLibraryAndExitThread((HMODULE)thisModule, 0);
#endif
	return nullptr;
}

void Unload()
{
	if (shuttingDown)
		return;
	shuttingDown = true;
	thread_t t;
	Threading::StartThread((threadFn)UnloadThread, (void*)&t, true, &t);
}
