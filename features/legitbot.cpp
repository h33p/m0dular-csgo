#include "legitbot.h"
#include "../core/fw_bridge.h"
#include "../core/settings.h"
#include "../sdk/framework/features/aimbot_types.h"
#include "../modules/perlin_noies/PerlinNoise.hpp"
#include <time.h>

siv::PerlinNoise perlinNoise(time(nullptr));

static float FirstParabola(float time)
{
	return 2.f * time * time;
}

static float SecondParabola(float time)
{
	return -2.f * (time - 1.f) * (time - 1.f) + 1.f;
}

//This new formula recreates distinct characteristics of human mouse movement, such as constant velocity change in one direction before sudden negation of acceleration (stopping the mouse)
//It is preferred to also noise the time value or the result in order to make it more like human (velocity changes are too consistent for a human hand to replicate)
static float BaseCurve(float time)
{
	return FirstParabola(time) + Min(Max(0.f, (time - 0.5f) / 0.000001f), 1.f) * (SecondParabola(time) - FirstParabola(time));
}

static float InvertedBaseCurve(float time)
{
	return 1.f - BaseCurve(1 - time);
}

//Old formula. Might be useful sometime
//return (std::cos(time * M_PI) + 1) * 0.5f * (1 + coefficient * time * time);

static float SmoothCurve(float time, float coefficient)
{
	return InvertedBaseCurve(powf(time, coefficient));
}

int prevUnsortID = -1;
float startAimTime = 0;
vec3_t startAngleDiff = vec3_t(0);
float startAngleDiffLen = 0;
float smoothCoefficient = 0;
float prevLerpTime = 0;
float aimTime = 0;
float lastAimTime = 0;
float aimEndTime = 0;
float targetTimeDelta = 1.f;
int aimedAt = 0;

static vec3_t prevAimOffset(0);
static vec3_t curAimOffset(0);

static float CurtimeRandom(float seed, float start, float end)
{
	return NormalizeFloat(globalVars->curtime * seed, start, end);
}

bool RunSmooth(LocalPlayer& lpData, AimbotTarget& target, int unsortID)
{

	//TODO: Allow to configure this
	if (globalVars->curtime - lastAimTime > Settings::legitBotAimResetTime)
		aimedAt = 0;

	if (unsortID < 0 || unsortID >= MAX_PLAYERS) {
		prevUnsortID = unsortID;
		return false;
	}

	vec3_t targetAngles = (target.targetVec - lpData.eyePos).GetAngles(true);

	//New target
	if (prevUnsortID != unsortID) {
		startAimTime = globalVars->curtime;
		//Be sure to apply the aimOffset so that RCS does not mess up
		startAngleDiff = (targetAngles - lpData.angles - curAimOffset).NormalizeAngles<2>(-180.f, 180.f);
		startAngleDiffLen = startAngleDiff.Length();
		smoothCoefficient = CurtimeRandom(13.3f, 1.f - 0.2f * (float)Settings::legitBotRandomization, 1.f + 0.8f * (float)Settings::legitBotRandomization);
		prevLerpTime = 0;
		//TODO Find a better way to calculate aim time
		aimTime = fminf(20.f, fmaxf(Settings::legitBotSpeed / (startAngleDiffLen * 0.1f), 5.f));
		targetTimeDelta = 1.f / aimTime;

		aimedAt++;

		//TODO: Implement a check for camera move velocity to detect what kind of delay is needed
		if (aimedAt > 1) {
			startAimTime += Settings::legitBotAimDelay + (float)Settings::legitBotAimDelay * (float)Settings::legitBotRandomization * CurtimeRandom(43.3f, -1.f, 1.f);
			targetTimeDelta = 1.f / aimTime + (float)Settings::legitBotShootDelay + (float)Settings::legitBotShootDelay * Settings::legitBotRandomization * CurtimeRandom(23.23f, -1.f, 1.f);
		}
	}

	prevUnsortID = unsortID;

	float timeDelta = (globalVars->curtime - startAimTime);

	float lTime = timeDelta * aimTime;

	if (timeDelta)
		lTime *= (1.f - Max(0.f, Min(1.f, 5.f - 5 * lTime)) * 0.4f * Settings::legitBotRandomization * perlinNoise.noise0_1(timeDelta * 300.f));

	float lerpTime = SmoothCurve(Max(0.f, Min(lTime, 1.f)), smoothCoefficient);

	//TODO: Allow moving the camera when we are in the delay aim stage. If we let it do now, recoil control will mess up
	if (timeDelta > 0.f || 1)
		lpData.angles = targetAngles - startAngleDiff.Lerp(vec3_t(0), lerpTime);
	else
		startAngleDiff = (targetAngles - lpData.angles - prevAimOffset).NormalizeAngles<2>(-180.f, 180.f);

	prevLerpTime = lerpTime;

	lpData.angles -= lpData.aimOffset;

	if (timeDelta < targetTimeDelta) {
		lpData.keys &= ~Keys::ATTACK1;
		lastAimTime = globalVars->curtime;
	} else
		aimEndTime = globalVars->curtime;

	return true;
}

static void RCS(LocalPlayer* lpData, bool smoothRun)
{
	vec3_t aimOffset = lpData->aimOffset;

	if (Settings::aimbotSetViewAngles || smoothRun) {
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

	curAimOffset = lpData->aimOffset;

	bool smoothRun = false;

#ifdef TESTING_FEATURES
	if (Settings::aimbotSetAngles)
		smoothRun = RunSmooth(*lpData, *target, target->id >= 0 ? (*track)[target->backTick].unsortIDs[target->id] : target->id);
#endif

	RCS(lpData, smoothRun);

	if (!Settings::aimbotSetAngles)
		lpData->angles = cmd->viewangles;
	else if (Settings::aimbotSetViewAngles)
		engine->SetViewAngles(lpData->angles);
}
