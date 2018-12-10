#ifndef ANTIAIM_H
#define ANTIAIM_H

#include "../sdk/features/types.h"

struct CUserCmd;

constexpr int FREESTAND_ANGLES = 36;
constexpr float ANGLE_STEP = 360.f / FREESTAND_ANGLES;

namespace Antiaim
{
	void Run(CUserCmd* cmd, FakelagState_t state);
	float CalculateFreestanding(int id, bool outAngles[FREESTAND_ANGLES] = nullptr);
}

#endif
