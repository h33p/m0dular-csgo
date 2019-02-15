#include "../sdk/framework/features/aimbot.h"

int minDamage = 10;

bool Aimbot::PreCompareData(AimbotTarget* target, LocalPlayer* localPlayer, vec3_t targetVec, int bone, float* outFOV)
{
	vec3_t angle = (targetVec - localPlayer->eyePos).GetAngles(true);
	vec3_t angleDiff = (shootAngles - angle).NormalizeAngles<2>(-180.f, 180.f);
	float fov = angleDiff.Length<2>();
	*outFOV = fov;
	return fov < target->fov;
}

bool Aimbot::CompareData(AimbotLoopData* d, int out, vec3_t targetVec, int bone, float fov)
{
	if (out < minDamage)
		return false;

	if (fov < d->fovs[d->entID])
		d->fovs[d->entID] = fov;

	if (fov < d->target.fov) {
		d->target.boneID = bone;
		d->target.targetVec = targetVec;
		d->target.dmg = out;
		d->target.fov = fov;
		return true;
	}
	return false;
}
