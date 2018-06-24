#include "../sdk/framework/g_defines.h"
#include "hooks.h"
#include "fw_bridge.h"

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

	FwBridge::UpdateLocalData(cmd, ****(void*****)__builtin_frame_address(0));
	FwBridge::UpdatePlayers(cmd);
	FwBridge::RunFeatures(cmd, bSendPacket);
	if (cmd->buttons & IN_ATTACK2)
		Unload();

	return false;
}
