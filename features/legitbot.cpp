#include "legitbot.h"
#include "../core/fw_bridge.h"
#include "../core/settings.h"
#include "../sdk/framework/features/aimbot_types.h"
#include "../modules/perlin_noies/PerlinNoise.hpp"
#include <time.h>

static vec3_t prevAimOffset;

static void RCS(LocalPlayer* lpData)
{
	vec3_t aimOffset = lpData->aimOffset;

	if (Settings::aimbotSetViewAngles) {
		lpData->angles -= aimOffset - prevAimOffset;
	} else
		lpData->angles -= aimOffset;

	prevAimOffset = aimOffset;
}

bool LegitBot::PreRun(LocalPlayer* lpData)
{

	return lpData->keys & Keys::ATTACK1;
}

void LegitBot::RunPostTarget(LocalPlayer* lpData, CUserCmd* cmd, AimbotTarget* target, HistoryList<Players, 64>* track)
{
	RCS(lpData);

	if (!Settings::aimbotSetAngles)
		lpData->angles = cmd->viewangles;
	else if (Settings::aimbotSetViewAngles)
		engine->SetViewAngles(lpData->angles);
}
