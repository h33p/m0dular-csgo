#include "../sdk/framework/g_defines.h"
#include "hooks.h"
#include "fw_bridge.h"

void Unload();

bool __fastcall SourceHooks::CreateMove(FASTARGS, float inputSampleTime, CUserCmd* cmd)
{
	static auto origFn = hookClientMode->GetOriginal(SourceHooks::CreateMove);

	bool* bSendPacket = nullptr;

	FwBridge::UpdateLocalData(cmd);
	FwBridge::UpdatePlayers(cmd);
	FwBridge::RunFeatures(cmd, bSendPacket);
	origFn(CFASTARGS, inputSampleTime, cmd);
	if (cmd->buttons & IN_ATTACK2)
		Unload();
	return false;
}
