#include "hooks.h"
#include "../framework/utils/threading.h"
#include "../framework/utils/memutils.h"
#include <atomic>
#include "../signatures.h"

#include "../framework/source_csgo/sdk.h"

VFuncHook* hookClientMode = nullptr;
VFuncHook* hookCl = nullptr;
VFuncHook* hookEngine = nullptr;

IClientMode* clientMode = nullptr;

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

int APIENTRY Construct(void* hModule, uintptr_t reasonForCall, void* lpReserved)
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
	Threading::FinishQueue();
}

static void InitializeHooks()
{

}
