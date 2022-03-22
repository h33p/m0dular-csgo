#ifndef ENGINE_H
#define ENGINE_H

#include "../sdk/framework/math/mmath.h"
#include "../sdk/framework/players.h"
#include "../sdk/source_csgo/sdk.h"
#include "macros.h"

extern CBaseClient* cl;
extern IClientMode* clientMode;
extern IVEngineClient* engine;
extern IClientEntityList* entityList;
extern CGlobalVarsBase* globalVars;
extern ICvar* cvar;
extern CGlowObjectManager* glowObjectManager;

extern bool* postProcessDisable;
extern int* smokeCount;

#define TICK_INTERVAL globalVars->interval_per_tick

inline float TicksToTime(int ticks)
{
	return ticks * TICK_INTERVAL;
}

inline int TimeToTicks(float time)
{
	return (int)(0.5f + (float)(time) / TICK_INTERVAL);
}

namespace Engine
{
	extern LocalPlayer lpData;
	extern C_BasePlayer* localPlayer;
	extern C_BaseCombatWeapon* activeWeapon;
	extern C_BasePlayer* playerList[MAX_PLAYERS];

	void UpdateLocalPlayer();
	void UpdateLocalData(CUserCmd* cmd);
	void RunFeatures(CUserCmd* cmd, float inputSampleTime);

	void FrameUpdate();
	bool IsEnemy(C_BasePlayer* ent);
	vec3_t PredictAimPunchAngle();
	vec2 GetMouseSensitivity();
}

#endif
