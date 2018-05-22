#include "hooks.h"
#include "../framework/utils/threading.h"
#include <atomic>

VFuncHook* hookClientMode = nullptr;
VFuncHook* hookCl = nullptr;
VFuncHook* hookEngine = nullptr;

static void InitializeOffsets();
static void InitializeHooks();

void* EntryPoint(void*)
{
	Threading::InitThreads();
	InitializeOffsets();
	InitializeHooks();
	return nullptr;
}

#if defined(__linux__) || defined(__APPLE__)
std::atomic_flag dlCloseLock = ATOMIC_FLAG_INIT;
std::atomic_bool isClosing(false);

__attribute__((constructor))
int Construct()
{
	pthread_attr_t tAttr;
	pthread_t thread;
	pthread_attr_init(&tAttr);
	pthread_attr_setdetachstate(&tAttr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread, &tAttr, EntryPoint, nullptr);
	return 0;
}

__attribute__((destructor))
void DLClose()
{
	isClosing = true;
	usleep(1000*1000);
}
#else
int APIENTRY DllMain(HMODULE hModule, uintptr_t reasonForCall, void* lpReserved)
{
	switch (reasonForCall) {
	  case DLL_PROCESS_ATTACH:
		  //thisHandle = hModule;
		  CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)EntryPoint, NULL, NULL, NULL);
		  break;
	  case DLL_PROCESS_DETACH:
		  break;
	}
	return 1;
}
#endif

static void InitializeOffsets()
{

}

static void InitializeHooks()
{

}
