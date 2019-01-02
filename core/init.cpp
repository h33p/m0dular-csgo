#include "hooks.h"
#include "fw_bridge.h"
#include "../sdk/framework/utils/threading.h"
#include "../sdk/framework/utils/shared_mutex.h"
#include "../sdk/framework/utils/stackstring.h"
#include "../sdk/framework/utils/memutils.h"
#include <atomic>
#include "../sdk/source_csgo/sdk.h"
#include "../sdk/source_shared/eventlistener.h"
#include "engine.h"
#include "impacts.h"

#include "../signatures.h"
#include "../hook_indices.h"
#include "../interfaces.h"
#include "tracing.h"
#include "settings.h"

#ifdef _WIN32
#include <d3d9.h>
#endif

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
IVDebugOverlay* debugOverlay = nullptr;
IMDLCache* mdlCache = nullptr;
CSpatialPartition* spatialPartition = nullptr;
IStaticPropMgr* staticPropMgr = nullptr;
CStaticPropMgr* staticPropMgrClient = nullptr;
IModelLoader* modelLoader = nullptr;
IPhysicsSurfaceProps* physProp = nullptr;

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

IntersectRayWithBoxFn IntersectRayWithBox = nullptr;
ClipRayToVPhysicsFn ClipRayToVPhysics = nullptr;

#ifdef __APPLE__
IntersectRayWithOBBFn IntersectRayWithOBB = nullptr;
CM_InlineModelNumberFn CM_InlineModelNumber = nullptr;
TransformedBoxTraceFn TransformedBoxTrace = nullptr;
#else
ClipRayToFn ClipRayToBSP = nullptr;
ClipRayToFn ClipRayToOBB = nullptr;
#endif

ThreadIDFn AllocateThreadID = nullptr;
ThreadIDFn FreeThreadID = nullptr;

EventListener listener({Impacts::ImpactEvent});

static void InitializeOffsets();
static void InitializeHooks();
static void InitializeDynamicHooks();
void Shutdown();
void Unload();

template<typename T, T& Fn>
static void DispatchToAllThreads(void*);

volatile bool cont = false;

void* __stdcall EntryPoint(void*)
{
#ifndef _WIN32
	//freopen("/tmp/csout.txt", "w", stdout);
#endif
	Threading::InitThreads();
#ifndef LOADER_INITIALIZATION
	InitializeOffsets();
#endif
    DispatchToAllThreads<ThreadIDFn, AllocateThreadID>(nullptr);
#ifndef LOADER_INITIALIZATION
	if (Settings::sharedInstance.initialized)
		InitializeHooks();
#endif
	cvar->ConsoleColorPrintf(Color(1.f, 1.f, 0.f, 1.f), ST("Initializing tracer as tracer...\n"));
	InitializeDynamicHooks();
	cvar->ConsoleColorPrintf(Color(1.f, 0.f, 0.f, 1.f), ST("ERROR: I'm already tracer!\n"));
	return nullptr;
}

Semaphore closeSemaphore;
#if defined(__linux__) || defined(__APPLE__)
#define APIENTRY __attribute__((constructor))
static thread_t* tThread = nullptr;

__attribute__((destructor))
void DLClose()
{
	cvar->ConsoleDPrintf(ST("dlclose called!\n"));
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

void SigOffset(const Signature* sig)
{
	*sig->result = PatternScan::FindPattern(sig->pattern, sig->module);
#ifdef DEBUG
	if (!*sig->result) {
		printf("Pattern scan fail on pattern %s [%s]\n", sig->pattern, sig->module);
		fflush(stdout);
	}
#endif
}

static void PlatformSpecificOffsets()
{
#ifdef __posix__
	uintptr_t hudUpdate = (*(uintptr_t**)cl)[11];
	globalVars = *(CGlobalVarsBase**)(GetAbsoluteAddress(hudUpdate + LWM(13, 0, 15), 3, 7));
	//uintptr_t activateMouse = (*(uintptr_t**)cl)[15];
	uintptr_t GetLocalPlayer = GetAbsoluteAddress((*(uintptr_t**)engine)[12] + 9, 1, 5);
	clientState = ((CClientState*(*)(int))GetLocalPlayer)(-1);

	MHandle handle = Handles::GetModuleHandle(ST("libSDL2-2.0"));

	uintptr_t polleventFn = (uintptr_t)(dlsym(handle, ST("SDL_PollEvent"))) OMac(+ 12);
	pollEventJump = (uintptr_t*)GetAbsoluteAddress(polleventFn, 3, 7);
	origPollEvent = *pollEventJump;

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

static void GatherTierExports()
{
	MHandle handle = Handles::GetModuleHandle(tierLib);

	AllocateThreadID = (ThreadIDFn)paddr(handle, ST("AllocateThreadID"));
	FreeThreadID = (ThreadIDFn)paddr(handle, ST("FreeThreadID"));
}

static void InitializeOffsets()
{
	for (size_t i = 0; i < sizeof(signatures) / (sizeof((signatures)[0])); i++)
		Threading::QueueJobRef(SigOffset, signatures + i);

	FindAllInterfaces(interfaceList, sizeof(interfaceList)/sizeof((interfaceList)[0]));
	FindVSTDFunctions();
	GatherTierExports();

	staticPropMgrClient = (CStaticPropMgr*)(staticPropMgr - 1);

	PlatformSpecificOffsets();
	SourceNetvars::Initialize(cl);
	Threading::FinishQueue();
}

static Semaphore dispatchSem;
static Semaphore waitSem;
static SharedMutex smtx;

template<typename T, T& Fn>
static void AllThreadsStub(void*)
{
	dispatchSem.Post();
	smtx.rlock();
	smtx.runlock();
	Fn();
}

//TODO: Build this into the threading library
template<typename T, T& Fn>
static void DispatchToAllThreads(void* data)
{
	smtx.wlock();

	for (size_t i = 0; i < Threading::numThreads; i++)
		Threading::QueueJobRef(AllThreadsStub<T, Fn>, data);

	for (size_t i = 0; i < Threading::numThreads; i++)
	    dispatchSem.Wait();

	smtx.wunlock();

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

	//Iniitalize input
#ifdef __posix__
	*pollEventJump = (uintptr_t)&CSGOHooks::PollEvent;
#else
	D3DDEVICE_CREATION_PARAMETERS params;

	if (d3dDevice && d3dDevice->GetCreationParameters(&params) == D3D_OK) {
		dxTargetWindow = params.hFocusWindow;
		oldWndProc = SetWindowLongPtr(dxTargetWindow, GWLP_WNDPROC, (LONG_PTR)CSGOHooks::WndProc);
	}
#endif
}

static void InitializeDynamicHooks()
{
	CSGOHooks::entityHooks = new std::unordered_map<C_BasePlayer*, VFuncHook*>();
	EffectsHook::HookAll(effectHooks, effectsCount, *effectsHead);
	SourceNetvars::HookAll(netvarHooks, netvarCount);
	listener.Initialize(ST("bullet_impact"));
	//Prevent the listener from accessing gameEvents on game shutdown
	listener.initialized = false;

#ifdef DEBUG
	cvar->ConsoleDPrintf(ST("Effect list:\n"));
	Color col = Color(0, 255, 0, 255);
    for (auto head = *effectsHead; head; head = head->next)
		cvar->ConsoleColorPrintf(col, ST("%s\n"), head->effectName);
#endif
}

static bool firstTime = true;

void Shutdown()
{
	if (firstTime) {

		firstTime = false;

		cvar->ConsoleDPrintf(ST("Ending threads...\n"));
		DispatchToAllThreads<ThreadIDFn, FreeThreadID>(nullptr);
		int ret = Threading::EndThreads();

		if (ret)
			cvar->ConsoleDPrintf(ST("Error ending threads! (%d)\n"), ret);

		cvar->ConsoleDPrintf(ST("Removing static hooks...\n"));
		for (HookDefine& i : hookIds) {
			if (i.hook) {
				delete i.hook;
				i.hook = nullptr;
			}
		}

#ifdef __posix__
		if (pollEventJump)
			*pollEventJump = origPollEvent;
#else
		if (oldWndProc)
		    SetWindowLongPtr(dxTargetWindow, GWLP_WNDPROC, oldWndProc);
#endif


		cvar->ConsoleDPrintf(ST("Removing entity hooks...\n"));
		if (CSGOHooks::entityHooks) {
			for (auto& i : *CSGOHooks::entityHooks)
				delete i.second;
			delete CSGOHooks::entityHooks;
			CSGOHooks::entityHooks = nullptr;
		}

		cvar->ConsoleDPrintf(ST("Removing effect hooks...\n"));
		EffectsHook::UnhookAll(effectHooks, effectsCount, *effectsHead);
		SourceNetvars::UnhookAll(netvarHooks, netvarCount);

		cvar->ConsoleDPrintf(ST("Shutting down engine...\n"));
		Engine::Shutdown();
		cvar->ConsoleDPrintf(ST("Shutting down tracer...\n"));
	}
}

void* __stdcall UnloadThread(thread_t* thisThread)
{
	Shutdown();

#if defined(__linux__) || defined(__APPLE__)
	tThread = thisThread;
	MHandle handle = Handles::GetPtrModuleHandle((void*)DLClose);

	//TODO: force ref count to 1 so it gets unloaded with a single call
	dlclose(handle);
	[[maybe_unused]] thread_t ctrd;
	//int count = 0;

	ctrd = Threading::StartThread((threadFn)dlclose, handle);

	cvar->ConsoleDPrintf(ST("Quitting thread!\n"));
	pthread_exit(0);
#else
	Sleep(50);
	cvar->ConsoleDPrintf(ST("Freeing library...\n"));
	FreeLibraryAndExitThread((HMODULE)thisModule, 0);
#endif
	return nullptr;
}

void Unload()
{
	if (shuttingDown)
		return;
	shuttingDown = true;
	//We are manually unloading, make sure we do destroy the event listener
	listener.initialized = true;
	thread_t t;
	Threading::StartThread((threadFn)UnloadThread, (void*)&t, true, &t);
}

InterfaceReg** GetInterfaceRegs(MHandle library)
{
#if defined(__linux__) || defined(__APPLE__)
    return (InterfaceReg**)dlsym(library, StackString("s_pInterfaceRegs"));
#elif defined(_WIN32)
	uintptr_t jmp = (uintptr_t)GetProcAddress(library, StackString("CreateInterface")) + 4;
    return *(InterfaceReg***)(GetAbsoluteAddress(jmp, 1, 5) + 6);
#endif
}
