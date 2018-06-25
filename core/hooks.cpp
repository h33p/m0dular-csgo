#include "../sdk/framework/g_defines.h"
#include "hooks.h"
#include "fw_bridge.h"
#include "engine.h"

void Unload();

bool __fastcall SourceHooks::CreateMove(FASTARGS, float inputSampleTime, CUserCmd* cmd)
{
	static auto origFn = hookClientMode->GetOriginal(SourceHooks::CreateMove);
	auto ret = origFn(CFASTARGS, inputSampleTime, cmd);

	//CL_ExtraMouseUpdate branch eventually calls clientMode createMove with null command_number and tick_count
	//Which we don't want to hook.
	if (!cmd->command_number || !cmd->tick_count)
		return ret;

	bool* bSendPacket = nullptr;

	FwBridge::inCreateMove = true;
	FwBridge::UpdateLocalData(cmd, ****(void*****)__builtin_frame_address(0));
	FwBridge::UpdatePlayers(cmd);
	FwBridge::RunFeatures(cmd, bSendPacket);
	FwBridge::inCreateMove = false;
	if (cmd->buttons & IN_ATTACK2)
		Unload();

	return false;
}

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
