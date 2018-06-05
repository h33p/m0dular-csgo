#include "../sdk/framework/g_defines.h"
#include "hooks.h"
#include "fw_bridge.h"

bool __fastcall SourceHooks::CreateMove(FASTARGS, float inputSampleTime, CUserCmd* cmd)
{
	static auto origFn = hookClientMode->GetOriginal(SourceHooks::CreateMove);
	FwBridge::UpdateLocalData(cmd);
	FwBridge::UpdatePlayers(cmd);
	origFn(CFASTARGS, inputSampleTime, cmd);
	return false;
}
