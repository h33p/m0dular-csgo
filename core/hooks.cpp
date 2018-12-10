#include "../sdk/framework/g_defines.h"
#include "hooks.h"
#include "fw_bridge.h"
#include "../sdk/framework/utils/stackstring.h"
#include "engine.h"
#include "visuals.h"
#include "impacts.h"
#include "tracing.h"
#include "../sdk/framework/utils/mutex.h"
#include "../sdk/framework/utils/threading.h"

void Unload();
extern bool shuttingDown;

[[gnu::flatten]]
bool __fastcall SourceHooks::CreateMove(FASTARGS, float inputSampleTime, CUserCmd* cmd)
{
	static auto origFn = hookClientMode->GetOriginal(SourceHooks::CreateMove);

	auto ret = origFn(CFASTARGS, inputSampleTime, cmd);

	//CL_ExtraMouseUpdate branch eventually calls clientMode createMove with null command_number and tick_count
	//Which we don't want to hook.
	if (shuttingDown || !cmd->command_number || !cmd->tick_count)
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

	FwBridge::UpdateLocalData(cmd, runFrameFp);
	FwBridge::UpdatePlayers(cmd);
	FwBridge::RunFeatures(cmd, bSendPacket, runFrameFp);

	if (cmd->buttons & IN_ATTACK2 && cmd->buttons & IN_JUMP && cmd->viewangles[0] > 85)
		Unload();

	return false;
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
	} else if (panelId == vpanel) {
		Visuals::Draw();
	}

	originalFunction(panel, vpanel, forceRepaint, allowForce);
}
#endif

std::unordered_map<C_BasePlayer*, VFuncHook*>* CSGOHooks::entityHooks = nullptr;

/*
  For the time being we do not hook this on windows. So, we are going to leak memory
*/

void __fastcall CSGOHooks::EntityDestruct(FASTARGS)
{
	VFuncHook* hook = entityHooks->at((C_BasePlayer*)thisptr);
	auto origFn = hook->GetOriginal(CSGOHooks::EntityDestruct);
	entityHooks->erase((C_BasePlayer*)thisptr);
	origFn(CFASTARGS);
}

bool __fastcall CSGOHooks::SetupBones(C_BasePlayer* thisptr, matrix3x4_t* matrix, int maxBones, int boneMask, float curtime)
{
	return ::SetupBones(thisptr, matrix, maxBones, boneMask, curtime);
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
	Engine::HandleLBYProxy((C_BasePlayer*)ent, data->value.Float);
}
