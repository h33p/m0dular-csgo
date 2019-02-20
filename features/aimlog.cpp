#include "aimlog.h"
#include "../core/fw_bridge.h"
#include "../sdk/framework/features/aimbot.h"
#include <vector>

struct AimData
{
	vec3_t endAnd;
	std::vector<vec3_t> angles;
};

static std::vector<vec3_t> angleHistory;
static std::vector<AimData> aimHistory;

void AimLog::LogCreateMove(const LocalPlayer& lpData, const AimbotTarget& target)
{
#ifdef DEBUG
	vec3_t targetAngles = (target.targetVec - lpData.eyePos).GetAngles(true);
	angleHistory.push_back(lpData.angles);
	if (target.id >= 0 && lpData.keys & Keys::ATTACK1) {
		if ((angleHistory[angleHistory.size() - 1] - targetAngles).NormalizeAngles<3>(-180.f, 180.f).Length<2>() <= 1.f) {
			aimHistory.push_back((AimData){targetAngles, angleHistory});

			if (angleHistory.size() > 2) {
				cvar->ConsoleDPrintf("Aim point history:\n");
				float prevDist = 9999999.f;
				float prevVel = 9999999.f;
				float prevAccel = 9999999.f;
				bool called = false;

				cvar->ConsoleDPrintf("X:\n");
				for (vec3_t i : angleHistory) {
					vec3_t angleDiff = (i - targetAngles).NormalizeAngles<3>(-180.f, 180.f);
					float dist = angleDiff[0]; //angleDiff.Length<2>();
					float vel = (prevDist - dist) / globalVars->interval_per_tick;
					float accel = (fabsf(vel) - fabsf(prevVel)) / globalVars->interval_per_tick;
					//float accelDiff = (accel - prevAccel) / globalVars->interval_per_tick;
					prevDist = dist;
					prevVel = vel;
					prevAccel = accel;
					if (called)
						cvar->ConsoleDPrintf("%.3f\n", dist);
					called = true;
				}

				called = false;
				cvar->ConsoleDPrintf("Y:\n");
				for (vec3_t i : angleHistory) {
					vec3_t angleDiff = (i - targetAngles).NormalizeAngles<3>(-180.f, 180.f);
					float dist = angleDiff[1]; //angleDiff.Length<2>();
					float vel = (prevDist - dist) / globalVars->interval_per_tick;
					float accel = (fabsf(vel) - fabsf(prevVel)) / globalVars->interval_per_tick;
					//float accelDiff = (accel - prevAccel) / globalVars->interval_per_tick;
					prevDist = dist;
					prevVel = vel;
					prevAccel = accel;
					if (called)
						cvar->ConsoleDPrintf("%.3f\n", dist);
					called = true;
				}
			}


			//Do not save it for now
			aimHistory.clear();
		}
		angleHistory.clear();
	}

	float diffs = 0;

	for (size_t i = angleHistory.size() - 2; i > 0 && i < angleHistory.size() && i > (long)angleHistory.size() - 20; i++) {
		diffs += angleHistory[i].DistTo(angleHistory[i + 1]);
	}

	if (angleHistory.size() > 20 && diffs < 1.f)
		angleHistory.clear();
#endif
}
