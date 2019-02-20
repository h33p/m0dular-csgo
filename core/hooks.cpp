#include "../sdk/framework/g_defines.h"
#include "hooks.h"
#include "fw_bridge.h"
#include "engine.h"
#include "tracing.h"
#include "settings.h"
#include "binds.h"
#include "mtr_scoped.h"
#include "../features/visuals.h"
#include "../features/impacts.h"
#include "../features/cameramodes.h"
#include "../features/nosmoke.h"
#include "../sdk/framework/utils/stackstring.h"
#include "../sdk/framework/utils/mutex.h"
#include "../sdk/framework/utils/threading.h"
#ifdef __posix__
#include <SDL2/SDL.h>
#else
#include <d3d9.h>
#include "../modules/keycode/keytable_win.c"
#endif

void Unload();
extern bool shuttingDown;

//This one is used when unloading
AtomicLock CSGOHooks::hookLock;

#ifdef __posix__
uintptr_t origPollEvent = 0;
uintptr_t* pollEventJump = nullptr;

int CSGOHooks::PollEvent(SDL_Event* event)
{
    auto OrigSDL_PollEvent = reinterpret_cast<decltype(CSGOHooks::PollEvent)*>(origPollEvent);

	if (event->type == SDL_KEYUP)
		BindManager::sharedInstance->binds[event->key.keysym.scancode].HandleKeyPress(false);
	else if (event->type == SDL_KEYDOWN)
		BindManager::sharedInstance->binds[event->key.keysym.scancode].HandleKeyPress(true);

	return OrigSDL_PollEvent(event);
}
#else
HWND dxTargetWindow;
LONG_PTR oldWndProc = 0;
IDirect3DDevice9* d3dDevice = nullptr;

LRESULT __stdcall CSGOHooks::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

	if (wParam >= 0 && wParam < 256) {
		if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
			BindManager::sharedInstance->binds[WIN_NATIVE_TO_HID[wParam]].HandleKeyPress(false);
		else if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
			BindManager::sharedInstance->binds[WIN_NATIVE_TO_HID[wParam]].HandleKeyPress(true);
	}

	return CallWindowProcW((WNDPROC)oldWndProc, hWnd, msg, wParam, lParam);
}

#endif

#ifdef MTR_ENABLED
static bool prevTraced = false;
#endif

[[gnu::flatten]]
bool __fastcall SourceHooks::CreateMove(FASTARGS, float inputSampleTime, CUserCmd* cmd)
{
	static auto origFn = hookClientMode->GetOriginal(SourceHooks::CreateMove);

	auto ret = origFn(CFASTARGS, inputSampleTime, cmd);

	FwBridge::localPlayer = nullptr;

	//CL_ExtraMouseUpdate branch eventually calls clientMode createMove with null command_number and tick_count
	//Which we don't want to hook.
	if (!cmd->command_number || !cmd->tick_count)
		return ret;

	if (shuttingDown || !CSGOHooks::hookLock.trylock())
		return ret;

	bool* bSendPacket = nullptr;
	void* runFrameFp = ****(void*****)FRAME_POINTER();

#if defined(__linux__)
	bSendPacket = **(bool***)FRAME_POINTER() - 0x18;
#elif defined(__APPLE__)
	bSendPacket = **(bool***)FRAME_POINTER() - 0x8;
#else
	bSendPacket = *(bool**)FRAME_POINTER() - 0x1C;
#endif

#ifdef MTR_ENABLED
	if (prevTraced != Settings::perfTrace) {
	    prevTraced = Settings::perfTrace;

		if (prevTraced) {
			mtr_start();
			cvar->ConsoleDPrintf("Starting trace\n");
		} else {
			mtr_stop();
			cvar->ConsoleDPrintf("Ending trace\n");
		}
	}
#endif

	MTR_BEGIN("Hooks", "CreateMove");
	//Settings::ipcLock->rlock();
	FwBridge::enableBoneSetup = true;
	Tracing2::ResetTraceCount();
	FwBridge::UpdateLocalData(cmd, runFrameFp);
	FwBridge::UpdatePlayers(cmd);
	FwBridge::RunFeatures(cmd, bSendPacket, runFrameFp);
	FwBridge::enableBoneSetup = false;
	//Settings::ipcLock->runlock();
	MTR_END("Hooks", "CreateMove");

	if (cmd->buttons & IN_ATTACK2 && cmd->buttons & IN_JUMP && cmd->viewangles[0] > 85)
		Unload();

	CSGOHooks::hookLock.unlock();

	return false;
}

void __fastcall CSGOHooks::OnRenderStart(FASTARGS)
{
	static auto origFn = hookViewRender->GetOriginal(CSGOHooks::OnRenderStart);
	origFn(CFASTARGS);

	if (!CSGOHooks::hookLock.trylock())
		return;

	MTR_SCOPED_TRACE("Hooks", "OnRenderStart");
	FwBridge::enableBoneSetup = true;
	FwBridge::UpdateLocalPlayer();
	Engine::FrameUpdate();
	FwBridge::enableBoneSetup = false;

	CSGOHooks::hookLock.unlock();
}

void __fastcall CSGOHooks::OverrideView(FASTARGS, CViewSetup* setup)
{
	static auto origFn = hookClientMode->GetOriginal(CSGOHooks::OverrideView);
	origFn(CFASTARGS, setup);

	if (!CSGOHooks::hookLock.trylock())
		return;

	MTR_SCOPED_TRACE("Hooks", "OverrideView");
	FwBridge::UpdateLocalPlayer();
	CameraModes::OverrideView(setup);
	NoSmoke::OnRenderStart();
	*postProcessDisable = Settings::disablePostProcessing;

	CSGOHooks::hookLock.unlock();
}

#ifdef PT_VISUALS
void __stdcall CSGOHooks::PaintTraverse(STDARGS PC vgui::VPANEL vpanel, bool forceRepaint, bool allowForce)
{
	static auto originalFunction = hookPanel->GetOriginal(void (__thiscall*)(void*, vgui::VPANEL, bool, bool), CSGOHooks::PaintTraverse);

	static vgui::VPANEL panelId = 0;

	const char* panelName = panel->GetName(vpanel);

	if (!panelId) {
		if (!strcmp(panelName, StackString("FocusOverlayPanel")))
			panelId = vpanel;
	} else if (panelId == vpanel && Settings::debugVisuals) {
		Visuals::Draw();
	}

	originalFunction(panel, vpanel, forceRepaint, allowForce);
}
#endif

std::unordered_map<C_BasePlayer*, VFuncHook*> CSGOHooks::entityHooks;

/*
  For the time being we do not hook this on windows. So, we are going to leak memory
*/

void __fastcall CSGOHooks::EntityDestruct(FASTARGS)
{
	VFuncHook* hook = entityHooks.at((C_BasePlayer*)thisptr);
	auto origFn = hook->GetOriginal(CSGOHooks::EntityDestruct);
	entityHooks.erase((C_BasePlayer*)thisptr);
	origFn(CFASTARGS);
}

bool __fastcall CSGOHooks::SetupBones(FASTARGS, matrix3x4_t* matrix, int maxBones, int boneMask, float curtime)
{
	C_BasePlayer* ent = (C_BasePlayer*)((uintptr_t*)thisptr - 1);

	//if (!FwBridge::enableBoneSetup)
	//	return false;
	//cvar->ConsoleDPrintf("SB: %p %p %d %x %f\n", thisptr, matrix, maxBones, boneMask, curtime);
	//cvar->ConsoleDPrintf("SB: %p\n", thisptr);
	if (!FwBridge::enableBoneSetup && Engine::CopyBones(ent, matrix, maxBones))
		return true;

	if (!FwBridge::enableBoneSetup)
		return false;

	//cvar->ConsoleDPrintf("SBA: %p %p %d %x %f\n", thisptr, matrix, maxBones, boneMask, curtime);
	VFuncHook* hook = entityHooks.at((C_BasePlayer*)thisptr);
	auto origFn = hook->GetOriginal(CSGOHooks::SetupBones);
	return origFn(CFASTARGS, matrix, maxBones, boneMask, curtime); //::SetupBones(thisptr, matrix, maxBones, boneMask, curtime);
}

extern EffectHook effectHooks[];
extern size_t effectsCount;

void CSGOHooks::ImpactsEffect(const CEffectData& effectData)
{
	static auto origFn = EffectsHook::GetOriginalCallback(effectHooks, effectsCount, CSGOHooks::ImpactsEffect);
	Impacts::HandleImpact(effectData);
	if (origFn)
		origFn(effectData);
}

void CSGOHooks::LBYProxy(const CRecvProxyData* data, void* ent, void* out)
{
    FwBridge::HandleLBYProxy((C_BasePlayer*)ent, data->value.Float);
}

void CSGOHooks::DidSmokeEffectProxy(const CRecvProxyData* data, void* ent, void* out)
{
	NoSmoke::HandleProxy(data, ent, out);
}
