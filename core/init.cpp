#include "hooks.h"
#include "../sdk/framework/utils/threading.h"
#include "../sdk/framework/utils/shared_mutex.h"
#include "../sdk/framework/utils/stackstring.h"
#include "../sdk/framework/utils/memutils.h"
#include <atomic>
#include "../sdk/source_csgo/sdk.h"
#include "engine.h"

#include "../bits/signatures.h"
#include "../bits/hook_indices.h"
#include "../bits/interfaces.h"

#include "../features/glow.h"

VFuncHook* hookClientMode = nullptr;

CBaseClient* cl = nullptr;
IClientMode* clientMode = nullptr;
IVEngineClient* engine = nullptr;
IClientEntityList* entityList = nullptr;
CGlobalVarsBase* globalVars = nullptr;
ICvar* cvar = nullptr;
CGlowObjectManager* glowObjectManager = nullptr;

bool* postProcessDisable = nullptr;
int* smokeCount = nullptr;

static void InitializeOffsets();
static void InitializeHooks();
void Shutdown(bool delayAfterUnhook = false);
void Unload();

volatile bool debuggerWait = false;

void* __stdcall EntryPoint(void*)
{
	while (debuggerWait)
		;

	Threading::InitThreads();
	InitializeOffsets();

	cvar->ConsoleColorPrintf(Color(1.f, 1.f, 0.f, 1.f), ST("Initializing tracer as tracer...\n"));
	InitializeHooks();
	cvar->ConsoleColorPrintf(Color(1.f, 0.f, 0.f, 1.f), ST("ERROR: I'm already tracer!\n"));

	return nullptr;
}

#if defined(__linux__) || defined(__APPLE__)
#define APIENTRY __attribute__((constructor))

// A little bit complex unloading dance
__attribute__((destructor))
void DLClose()
{
	cvar->ConsoleDPrintf(ST("dlclose called!\n"));
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
	*sig->result = PatternScan::FindPattern(sig->pattern, sig->module);
}

static void PlatformSpecificOffsets()
{
#ifdef __posix__
	uintptr_t hudUpdate = (*(uintptr_t**)cl)[11];
	globalVars = *(CGlobalVarsBase**)(GetAbsoluteAddress(hudUpdate + LWM(13, 0, 15), 3, 7));
#else
	globalVars = **(CGlobalVarsBase***)((*(uintptr_t**)(cl))[0] + 0x1B);
#endif
}

static void InitializeOffsets()
{
	for (size_t i = 0; i < sizeof(signatures) / (sizeof((signatures)[0])); i++)
		Threading::QueueJobRef(SigOffset, signatures + i);

	FindAllInterfaces(interfaceList, sizeof(interfaceList)/sizeof((interfaceList)[0]));

	PlatformSpecificOffsets();

	SourceNetvars::Initialize(cl);

	Threading::FinishQueue(true);
}

static void InitializeHooks()
{
	//We have to specify the minSize since vtables on MacOS act strangely with one or two functions being a null pointer
	hookClientMode = new VFuncHook(clientMode, false, 25);

	for (size_t i = 0; i < sizeof(hookIds) / sizeof((hookIds)[0]); i++)
		hookIds[i].hook->Hook(hookIds[i].index, hookIds[i].function);
}

static bool firstTime = true;

void Shutdown(bool delayAfterUnhook)
{
	if (firstTime) {

		firstTime = false;

		cvar->ConsoleDPrintf(ST("Removing netvar hooks...\n"));
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

		cvar->ConsoleDPrintf(ST("Shutting down glow...\n"));
		Glow::Shutdown();
		cvar->ConsoleDPrintf(ST("Shutting down tracer...\n"));

		int ret = Threading::EndThreads();

		if (ret)
			cvar->ConsoleDPrintf(ST("Error ending threads! (%d)\n"), ret);
	}
}

// Missing peace to complete sdk/source_shared/interfaces.h
//
// Define interfaces in bits/interfaces.h
InterfaceReg** GetInterfaceRegs(MHandle library)
{
#if defined(__linux__) || defined(__APPLE__)
	return (InterfaceReg**)dlsym(library, StackString("s_pInterfaceRegs"));
#elif defined(_WIN32)
	uintptr_t jmp = (uintptr_t)GetProcAddress(library, StackString("CreateInterface")) + 4;
	return *(InterfaceReg***)(GetAbsoluteAddress(jmp, 1, 5) + 6);
#endif
}
