#include "hooks.h"
#include "platform_hooks.h"
#include "fw_bridge.h"
#include "../sdk/framework/utils/threading.h"
#include "../sdk/framework/utils/shared_mutex.h"
#include "../sdk/framework/utils/stackstring.h"
#include "../sdk/framework/utils/memutils.h"
#include <atomic>
#include "../sdk/source_csgo/sdk.h"
#include "../sdk/source_shared/eventlistener.h"
#include "engine.h"

#include "../bits/signatures.h"
#include "../bits/hook_indices.h"
#include "../bits/interfaces.h"
#include "../bits/identify.h"
#include "tracing.h"
#include "settings.h"
#include "mtr_scoped.h"

#include "../features/impacts.h"
#include "../features/glow.h"

#ifdef _WIN32
#include <d3d9.h>
#include "../gui/menu/dx9menu.h"

namespace PSMenu = DX9Menu;

#else
#include "../gui/menu/sdlmenu.h"
#include <SDL2/SDL.h>

namespace PSMenu = SDLMenu;

//TODO: Move these out to separate file
extern SDL_Window* lastWindow;
extern SDL_GLContext imguiContext;
#endif

VFuncHook* hookClientMode = nullptr;
VFuncHook* hookPanel = nullptr;
VFuncHook* hookViewRender = nullptr;
VFuncHook* hookSurface = nullptr;
#ifdef _WIN32
VFuncHook* hookD3D = nullptr;
#endif

CBaseClient* cl = nullptr;
CServerGame* server = nullptr;
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
CInput* input = nullptr;
CSGameMovement* gameMovement = nullptr;
CGlowObjectManager* glowObjectManager = nullptr;
CCommonHostState* hostState = nullptr;
IStudioRenderContext* studioRender = nullptr;

CL_RunPredictionFn CL_RunPrediction = nullptr;
Weapon_ShootPositionFn Weapon_ShootPosition = nullptr;
RunSimulationFn RunSimulationFunc = nullptr;
GetWeaponInfoFn GetWeaponInfo = nullptr;
SetAbsFn SetAbsOrigin = nullptr;
SetAbsFn SetAbsAngles = nullptr;
SetAbsFn SetAbsVelocity = nullptr;
SetupBonesFn SetupBones = nullptr;
int* modelBoneCounter = nullptr;
IsBreakableEntityFn IsBreakableEntityNative = nullptr;
bool* postProcessDisable = nullptr;
int* smokeCount = nullptr;

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
void Shutdown(bool delayAfterUnhook = false);
void Unload();

template<typename T, T& Fn>
static void DispatchToAllThreads(void*);

volatile bool cont = false;
static volatile char* moduleIdentifyDependency = nullptr;
static volatile char* moduleIdentifyDependency2 = nullptr;

void* __stdcall EntryPoint(void*)
{
	while (0 && !cont)
		;
#ifdef MTR_ENABLED
#ifdef _WIN32
	mtr_init("C:\\Temp\\csgotrace.json");
#else
	mtr_init("/tmp/csgotrace.json");
#endif
	MTR_META_PROCESS_NAME("thread_pool_test");
#endif
	MTR_SCOPED_TRACE("Initialization", "EntryPoint");
#ifdef DEBUG_STDOUT
#ifndef _WIN32
	freopen("/tmp/csout.txt", "w", stdout);
#else
	freopen("C:\\temp\\csout.txt", "w", stdout);
#endif
	printf("Initialize stdout\n");
	fflush(stdout);
#endif
	Threading::InitThreads();
#ifndef LOADER_INITIALIZATION
	InitializeOffsets();
#endif
	moduleIdentifyDependency = (volatile char*)GetModuleName((void*)RandomFloatExp, (void*)RandomFloatExp);
	DispatchToAllThreads<ThreadIDFn, AllocateThreadID>(nullptr);

	Settings::sharedInstance.Initialize();

#ifndef LOADER_INITIALIZATION
	if (Settings::sharedInstance.initialized)
		InitializeHooks();
#endif
	cvar->ConsoleColorPrintf(Color(1.f, 1.f, 0.f, 1.f), ST("Initializing tracer as tracer...\n"));
	moduleIdentifyDependency2 = (volatile char*)moduleName;
	delete (char*)moduleIdentifyDependency;
	InitializeDynamicHooks();
	cvar->ConsoleColorPrintf(Color(1.f, 0.f, 0.f, 1.f), ST("ERROR: I'm already tracer!\n"));
#ifdef MTR_ENABLED
	mtr_stop();
#endif
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
	if (reasonForCall == DLL_PROCESS_DETACH)
		Shutdown(true);
	else if (reasonForCall == DLL_PROCESS_ATTACH)
#endif
		Threading::StartThread(EntryPoint, NULL);
	return 1;
}

void SigOffset(const Signature* sig)
{
	MTR_SCOPED_TRACE("Initialization", "SigOffset");
	*sig->result = PatternScan::FindPattern(sig->pattern, sig->module);
#ifdef DEBUG
	if (!*sig->result) {
		printf("Pattern scan fail on pattern %s [%s]\n", sig->pattern, sig->module);
		fflush(stdout);
	} else {
		printf("Pattern scan success on pattern %s [%s]\n", sig->pattern, sig->module);
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
#ifdef __linux__
	pollEventJump = (uintptr_t*)GetAbsoluteAddress(polleventFn, 2, 6);
#else
	pollEventJump = (uintptr_t*)GetAbsoluteAddress(polleventFn, 3, 7);
#endif
	origPollEvent = *pollEventJump;

	uintptr_t swapwindowFn = (uintptr_t)(dlsym(handle, ST("SDL_GL_SwapWindow"))) OMac(+ 12);
#ifdef __linux__
	swapWindowJump = (uintptr_t*)GetAbsoluteAddress(swapwindowFn, 2, 6);
#else
	swapWindowJump = (uintptr_t*)GetAbsoluteAddress(swapwindowFn, 3, 7);
#endif
	origSwapWindow = *swapWindowJump;

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
	MTR_SCOPED_TRACE("Initialization", "InitializeOffsets");

	for (size_t i = 0; i < sizeof(signatures) / (sizeof((signatures)[0])); i++)
		Threading::QueueJobRef(SigOffset, signatures + i);

	FindAllInterfaces(interfaceList, sizeof(interfaceList)/sizeof((interfaceList)[0]));
	FindVSTDFunctions();
	GatherTierExports();

	staticPropMgrClient = (CStaticPropMgr*)(staticPropMgr - 1);

	PlatformSpecificOffsets();
	MTR_BEGIN("Netvars", "ServerInitialize");
	SourceNetvars::InitializeServer(server);
	MTR_END("Netvars", "ServerInitialize");
	MTR_BEGIN("Netvars", "Initialize");
	SourceNetvars::Initialize(cl);
	MTR_END("Netvars", "Initialize");
	Threading::FinishQueue(true);

#ifdef DEBUG
	for (const Signature& sig : offsetSignatures)
		cvar->ConsoleDPrintf("%s:\t%lx\n", sig.result, PatternScan::FindPattern(sig.pattern, sig.module));

	for (const NetvarOffsetSignature& sig : netvarOffsetSignatures) {
		int off = (int)PatternScan::FindPattern(sig.pattern, sig.module);
		uintptr_t var = SourceNetvars::GetNearestNetvar(sig.dataTable, off);
		int varOff = SourceNetvars::GetOffset(sig.dataTable, var);
		int x = off - varOff;
		const char* varName = SourceNetvars::GetName(sig.dataTable, var);
		cvar->ConsoleDPrintf("%s:\t%x = %s %c %x\n", sig.result, off, varName, x < 0 ? '-' : '+', x < 0 ? -x : x);
	}

	for (const Signature& sig : indexSignatures)
		cvar->ConsoleDPrintf("%s:\t%ld\n", sig.result, PatternScan::FindPattern(sig.pattern, sig.module) / sizeof(uintptr_t));
#endif
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

	Threading::FinishQueue(false);
}

static void InitializeHooks()
{
	MTR_SCOPED_TRACE("Initialization", "InitializeHooks");
	//We have to specify the minSize since vtables on MacOS act strangely with one or two functions being a null pointer
	hookClientMode = new VFuncHook(clientMode, false, 25);
	hookViewRender = new VFuncHook(viewRender, false, 10);
	hookSurface = new VFuncHook(surface, false, 68);
#ifdef PT_VISUALS
	hookPanel = new VFuncHook(panel, false, 5);
#endif

	//Iniitalize input
#ifdef __posix__
	if (pollEventJump != nullptr)
		*pollEventJump = (uintptr_t)&PlatformHooks::PollEvent;
	if (swapWindowJump != nullptr)
		*swapWindowJump = (uintptr_t)&PlatformHooks::SwapWindow;
#else
	D3DDEVICE_CREATION_PARAMETERS params;

	hookD3D = new VFuncHook(d3dDevice, false, 18);

	if (d3dDevice && d3dDevice->GetCreationParameters(&params) == D3D_OK) {
		dxTargetWindow = params.hFocusWindow;
		oldWndProc = SetWindowLongPtr(dxTargetWindow, GWLP_WNDPROC, (LONG_PTR)PlatformHooks::WndProc);
	}
#endif

	for (size_t i = 0; i < sizeof(hookIds) / sizeof((hookIds)[0]); i++)
		hookIds[i].hook->Hook(hookIds[i].index, hookIds[i].function);
}

static void InitializeDynamicHooks()
{
	MTR_SCOPED_TRACE("Initialization", "InitializeDynamicHooks");
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

void Shutdown(bool delayAfterUnhook)
{
	if (firstTime) {

		firstTime = false;

#ifdef __posix__
		if (pollEventJump)
			*pollEventJump = origPollEvent;

		if (swapWindowJump)
			*swapWindowJump = origSwapWindow;
#else
		if (oldWndProc)
			SetWindowLongPtr(dxTargetWindow, GWLP_WNDPROC, oldWndProc);
#endif


		cvar->ConsoleDPrintf(ST("Removing entity hooks...\n"));
		for (auto& i : CSGOHooks::entityHooks)
			delete i.second;
		CSGOHooks::entityHooks.clear();

		cvar->ConsoleDPrintf(ST("Removing effect hooks...\n"));
		EffectsHook::UnhookAll(effectHooks, effectsCount, *effectsHead);
		SourceNetvars::UnhookAll(netvarHooks, netvarCount);

		cvar->ConsoleDPrintf(ST("Removing static hooks...\n"));
		for (HookDefine& i : hookIds) {
			if (i.hook) {
				delete i.hook;
				i.hook = nullptr;
			}
		}

		if (delayAfterUnhook) {
#ifdef _WIN32
			Sleep(100);
#else
			usleep(100000);
#endif
		}

		//Lock hooks before shutting down the cheat, since there is a high chance a hook is running at this exact moment
		CSGOHooks::hookLock.lock();
		CSGOHooks::hookLock.unlock();
		PlatformHooks::hookLock.lock();
		PlatformHooks::hookLock.unlock();

		cvar->ConsoleDPrintf(ST("Shutting down gui...\n"));
#ifdef __posix__
		SDL_GLContext originalContext = SDL_GL_GetCurrentContext();

		if (lastWindow)
			SDL_GL_MakeCurrent(lastWindow, imguiContext);
#endif

		PSMenu::ShutdownContext();

#ifdef __posix__
		if (lastWindow)
			SDL_GL_MakeCurrent(lastWindow, originalContext);
#endif

		cvar->ConsoleDPrintf(ST("Shutting down engine...\n"));
		Engine::Shutdown();
		cvar->ConsoleDPrintf(ST("Shutting down glow...\n"));
		Glow::Shutdown();
		cvar->ConsoleDPrintf(ST("Shutting down tracer...\n"));

		DispatchToAllThreads<ThreadIDFn, FreeThreadID>(nullptr);
		int ret = Threading::EndThreads();

		if (ret)
			cvar->ConsoleDPrintf(ST("Error ending threads! (%d)\n"), ret);

#ifdef MTR_ENABLED
		mtr_flush();
		mtr_shutdown();
		cvar->ConsoleDPrintf("Ending trace\n");
#endif

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
