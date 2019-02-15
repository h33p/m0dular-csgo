#ifndef SPREAD_H
#define SPREAD_H

struct Players;
struct CUserCmd;

#include "../sdk/framework/features/aimbot_types.h"

namespace Spread
{
	bool HitChance(Players* players, int targetEnt, vec3_t targetVec, int boneID, int chance);
	void CompensateSpread(CUserCmd* cmd);
}

#endif
