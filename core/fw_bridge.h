#ifndef FW_BRIDGE_H
#define FW_BRIDGE_H
/*
 * Here we implement game specific functions
 * to bridge the game with the framework
*/

#include "../sdk/framework/math/mmath.h"
#include "../sdk/framework/players.h"
#include "../sdk/framework/utils/history_list.h"
#include "../sdk/source_csgo/sdk.h"
#include "macros.h"

#if defined(__linux__)
#define RUNFRAME_TICK 0x2
#define RUNFRAME_SERVERTICK 0x6
#elif defined(__APPLE__)
#define RUNFRAME_TICK 0xb
#define RUNFRAME_SERVERTICK 0x1
#else
#define RUNFRAME_TICK 0x6
#define RUNFRAME_SERVERTICK 0x15
#endif

extern CBaseClient* cl;
extern IClientMode* clientMode;
extern IVEngineClient* engine;
extern IClientEntityList* entityList;
extern CGlobalVarsBase* globalVars;
extern IVModelInfo* mdlInfo;
extern IEngineTrace* engineTrace;
extern ICvar* cvar;
extern CClientState* clientState;
extern CPrediction* prediction;
extern IPanel* panel;
extern ISurface* surface;
extern IViewRender* viewRender;
extern void* weaponDatabase;
extern CClientEffectRegistration** effectsHead;

typedef void (*CL_RunPredictionFn)(void);
typedef vec3(__thiscall*Weapon_ShootPositionFn)(void*);
typedef CCSWeaponInfo*(__thiscall*GetWeaponInfoFn)(void*, ItemDefinitionIndex);
typedef void(__thiscall* SetAbsFn)(void*, const vec3& origin);
typedef bool(__thiscall* SetupBonesFn)(C_BasePlayer*, matrix3x4_t*, int, int, float);

#ifdef _WIN32
typedef void(__vectorcall*RunSimulationFn)(void*, void*, float, float, float, int, CUserCmd*, C_BaseEntity*);
#else
typedef void(*RunSimulationFn)(void*, float, int, CUserCmd*, C_BaseEntity*);
#endif

typedef void (*RandomSeedFn)(int);
typedef float (*RandomFloatFn)(float, float);
typedef float (*RandomFloatExpFn)(float, float, float);
typedef int (*RandomIntFn)(int, int);
typedef float (*RandomGaussianFloatFn)(float, float);

extern CL_RunPredictionFn CL_RunPrediction;
extern Weapon_ShootPositionFn Weapon_ShootPosition;
extern RunSimulationFn RunSimulationFunc;
extern GetWeaponInfoFn GetWeaponInfo;
extern SetAbsFn SetAbsOrigin;
extern SetAbsFn SetAbsAngles;
extern SetAbsFn SetAbsVelocity;
extern SetupBonesFn SetupBones;

extern RandomSeedFn RandomSeed;
extern RandomFloatFn RandomFloat;
extern RandomFloatExpFn RandomFloatExp;
extern RandomIntFn RandomInt;
extern RandomGaussianFloatFn RandomGaussianFloat;

#define TICK_INTERVAL globalVars->interval_per_tick

inline float TicksToTime(int ticks)
{
	return ticks * TICK_INTERVAL;
}

inline int TimeToTicks(float time)
{
	return (int)(0.5f + (float)(time) / TICK_INTERVAL);
}

namespace FwBridge
{
	extern HistoryList<Players, BACKTRACK_TICKS> playerTrack;
	extern LocalPlayer lpData;
	extern C_BasePlayer* localPlayer;
	extern C_BaseCombatWeapon* activeWeapon;
	extern float maxBacktrack;
	extern int hitboxIDs[];
	void UpdatePlayers(CUserCmd* cmd);
	void UpdateLocalData(CUserCmd* cmd, void* hostRunFrameFp);
	void RunFeatures(CUserCmd* cmd, bool* bSendPacket);
}

#endif
