#ifndef SPREAD_H
#define SPREAD_H

#include "fw_bridge.h"
#include "../sdk/framework/features/aimbot.h"

namespace Spread
{
	bool HitChance(Players* players, int targetEnt, vec3_t targetVec, int boneID);
	void CompensateSpread(CUserCmd* cmd);
}

#endif
