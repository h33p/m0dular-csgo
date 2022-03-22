#include "../sdk/framework/g_defines.h"
#include "hooks.h"
#include "engine.h"
#include "../features/nosmoke.h"
#include "../features/glow.h"
#include "../sdk/framework/utils/stackstring.h"
#include "../sdk/framework/utils/mutex.h"
#include "../sdk/framework/utils/threading.h"
#include "source_features.h"
#ifdef __posix__
#include <SDL2/SDL.h>
#else
#include <d3d9.h>
#include "../modules/keycode/keytable_win.c"
#endif

void Unload();
extern bool shuttingDown;

[[gnu::flatten]]
bool __fastcall SourceHooks::CreateMove(FASTARGS, float inputSampleTime, CUserCmd* cmd)
{
	static auto origFn = hookClientMode->GetOriginal(SourceHooks::CreateMove);

	auto ret = origFn(CFASTARGS, inputSampleTime, cmd);

	Engine::localPlayer = nullptr;

	//CL_ExtraMouseUpdate branch eventually calls clientMode createMove with null command_number and tick_count
	//Which we don't want to hook.
	if (!cmd->command_number || !cmd->tick_count)
		return ret;

	Engine::UpdateLocalData(cmd);

	//SourceBhop::Run(cmd, &Engine::lpData);
	//SourceAutostrafer::Run(cmd, &Engine::lpData, 1.3f);
	SourceEssentials::UpdateCMD(cmd, &Engine::lpData, Engine::GetMouseSensitivity(), inputSampleTime);

	return false;
}

bool disablePostProcessing = false;

void __fastcall CSGOHooks::OverrideView(FASTARGS, CViewSetup* setup)
{
	static auto origFn = hookClientMode->GetOriginal(CSGOHooks::OverrideView);
	origFn(CFASTARGS, setup);

	NoSmoke::OnRenderStart();
	*postProcessDisable = disablePostProcessing;
}

int __fastcall CSGOHooks::DoPostScreenSpaceEffects(FASTARGS, CViewSetup* setup)
{
	static auto origFn = hookClientMode->GetOriginal(CSGOHooks::DoPostScreenSpaceEffects);

	Engine::UpdateLocalPlayer();
	Glow::Run();

	return origFn(CFASTARGS, setup);
}

void CSGOHooks::DidSmokeEffectProxy(const CRecvProxyData* data, void* ent, void* out)
{
	NoSmoke::HandleProxy(data, ent, out);
}
