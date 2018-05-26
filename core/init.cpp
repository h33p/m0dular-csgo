#include "hooks.h"
#include "fw_bridge.h"
#include "../framework/utils/threading.h"
#include "../framework/utils/memutils.h"
#include <atomic>
#include "../framework/source_csgo/sdk.h"

#include "../signatures.h"
#include "../hook_indices.h"
#include "../interfaces.h"

VFuncHook* hookClientMode = nullptr;

CBaseClient* cl = nullptr;
IClientMode* clientMode = nullptr;
IVEngineClient* engine = nullptr;
IClientEntityList* entityList = nullptr;
CGlobalVarsBase* globalVars = nullptr;
IVModelInfo* mdlInfo = nullptr;

static void InitializeOffsets();
static void InitializeHooks();
void Shutdown();
void Unload();

void* __stdcall EntryPoint(void*)
{
	Threading::InitThreads();
#ifndef LOADER_INITIALIZATION
	InitializeOffsets();
	InitializeHooks();
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
	Shutdown();
	closeSemaphore.Post();
	void* ret;
	pthread_join(*tThread, &ret);
}
#endif

int APIENTRY DllMain(void* hModule, uintptr_t reasonForCall, void* lpReserved)
{
#ifdef _WIN32
	if (reasonForCall == DLL_PROCESS_ATTACH)
#endif
		Threading::StartThread(EntryPoint, NULL);
	return 1;
}

struct SigOut
{
	signature_t sig;
	uintptr_t* var;
	SigOut(uintptr_t* v, signature_t s) : sig(s), var(v) { }
};

void SigOffset(SigOut* sig)
{
	*sig->var = PatternScan::FindPattern(sig->sig.pattern, sig->sig.module);
}

static void PlatformSpecificOffsets()
{
#ifdef __posix__

	uintptr_t hudUpdate = (*(uintptr_t**)cl)[11];
	globalVars = *(CGlobalVarsBase**)(GetAbsoluteAddress(hudUpdate + LWM(13, 0, 15), 3, 7));
	uintptr_t activateMouse = (*(uintptr_t**)cl)[15];

#else
	**(CGlobalVarsBase***)((*(uintptr_t**)(cl))[0] + 0x1B);
#endif
}

static void InitializeOffsets()
{
	Threading::QueueJob(SigOffset, SigOut((uintptr_t*)&clientMode, clientModeSig));

	FindAllInterfaces(interfaceList, sizeof(interfaceList)/sizeof((interfaceList)[0]));
	PlatformSpecificOffsets();
	SourceNetvars::Initialize(cl);
	Threading::FinishQueue();
}

static void InitializeHooks()
{
	//We have to specify the minSize since vtables on MacOS act strangely with one or two functions being a null pointer
	hookClientMode = new VFuncHook(clientMode, false, 25);

	for (int i = 0; i < sizeof(hookIds) / sizeof((hookIds)[0]); i++)
		hookIds[i].hook->Hook(hookIds[i].index, hookIds[i].function);
}

void Shutdown()
{

	Threading::EndThreads();

	if (hookClientMode) {
		delete hookClientMode;
		hookClientMode = nullptr;
	}
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
	while (closeSemaphore.TimedWait(20)) {
		if (count++)
			pthread_cancel(ctrd);
		ctrd = Threading::StartThread((threadFn)dlclose, handle);
	}
#else

#endif
	return nullptr;
}

void Unload()
{
	thread_t t;
	Threading::StartThread((threadFn)UnloadThread, (void*)&t, &t);
}
