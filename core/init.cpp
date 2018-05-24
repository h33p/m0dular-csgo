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

IClientMode* clientMode = nullptr;
IVEngineClient* engine = nullptr;
IClientEntityList* entityList = nullptr;

static void InitializeOffsets();
static void InitializeHooks();

void* __stdcall EntryPoint(void*)
{
	Threading::InitThreads();
#ifndef LOADER_INITIALIZATION
	InitializeOffsets();
	InitializeHooks();
#endif
	return nullptr;
}

#if defined(__linux__) || defined(__APPLE__)
#define APIENTRY __attribute__((constructor))
std::atomic_flag dlCloseLock = ATOMIC_FLAG_INIT;
std::atomic_bool isClosing(false);

__attribute__((destructor))
void DLClose()
{
	isClosing = true;
	usleep(1000*1000);
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

static void InitializeOffsets()
{
	Threading::QueueJob(SigOffset, SigOut((uintptr_t*)&clientMode, clientModeSig));

	FindAllInterfaces(interfaceList, sizeof(interfaceList)/sizeof((interfaceList)[0]));
	Threading::FinishQueue();
}

static void InitializeHooks()
{
	//We have to specify the minSize since vtables on MacOS act strangely with one or two functions being a null pointer
	hookClientMode = new VFuncHook(clientMode, false, 25);

	for (int i = 0; i < sizeof(hookIds) / sizeof((hookIds)[0]); i++)
		hookIds[i].hook->Hook(hookIds[i].index, hookIds[i].function);
}
