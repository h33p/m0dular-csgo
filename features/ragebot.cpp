#include "ragebot.h"
#include "spread.h"
#include "../core/fw_bridge.h"
#include "../core/settings.h"
#include "../sdk/framework/features/aimbot_types.h"

bool RageBot::PreRun(LocalPlayer* lpData)
{
	return FwBridge::activeWeapon->nextPrimaryAttack() <= globalVars->curtime && (Settings::aimbotAutoShoot || lpData->keys & Keys::ATTACK1);
}

static vec3_t prevAimOffset(0);

void RageBot::RunPostTarget(LocalPlayer* lpData, CUserCmd* cmd, AimbotTarget* target, HistoryList<Players, 64>* track)
{
	vec3_t targetAngles = (target->targetVec - lpData->eyePos).GetAngles(true);

	if (target->id >= 0) {
		prevAimOffset = vec3_t(0);

		lpData->angles = targetAngles;

		if (Settings::aimbotAutoShoot)
			lpData->keys |= Keys::ATTACK1;

		if (lpData->keys & Keys::ATTACK1)
			Spread::CompensateSpread(cmd);
	}


	//RCS
	if (Settings::aimbotSetViewAngles) {
		lpData->angles -= lpData->aimOffset - prevAimOffset;
	} else
		lpData->angles -= lpData->aimOffset;

	if (!Settings::aimbotSetAngles)
		lpData->angles = cmd->viewangles;
	else if (Settings::aimbotSetViewAngles)
	    engine->SetViewAngles(lpData->angles);

	prevAimOffset = lpData->aimOffset;
}
