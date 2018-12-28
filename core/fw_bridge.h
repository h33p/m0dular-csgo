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
#include "../sdk/framework/features/aimbot.h"
#include "../sdk/framework/utils/settings.h"
#include "macros.h"

#if defined(__linux__)
#define RUNFRAME_TICK 0x3
#define RUNFRAME_SERVERTICK 0x4
#elif defined(__APPLE__)
#define RUNFRAME_TICK 0xb
#define RUNFRAME_SERVERTICK 0x1
#else
#define RUNFRAME_TICK 0x6
#define RUNFRAME_SERVERTICK 0x10
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
extern IGameEventManager* gameEvents;
extern IVDebugOverlay* debugOverlay;
extern IMDLCache* mdlCache;
extern CSpatialPartition* spatialPartition;
extern IStaticPropMgr* staticPropMgr;
extern CStaticPropMgr* staticPropMgrClient;
extern IModelLoader* modelLoader;
extern IPhysicsSurfaceProps* physProp;

typedef void (*CL_RunPredictionFn)(void);
#ifdef _WIN32
typedef void(__thiscall* Weapon_ShootPositionFn)(void*, vec3_t&);
#else
typedef vec3(__thiscall* Weapon_ShootPositionFn)(void*);
#endif
typedef CCSWeaponInfo*(__thiscall*GetWeaponInfoFn)(void*, ItemDefinitionIndex);
typedef void(__thiscall* SetAbsFn)(void*, const vec3& origin);
typedef bool(__thiscall* SetupBonesFn)(C_BasePlayer*, matrix3x4_t*, int, int, float);

#ifdef _WIN32
typedef void(__vectorcall* RunSimulationFn)(void*, void*, float, float, float, int, CUserCmd*, C_BaseEntity*);
#else
typedef void(*RunSimulationFn)(void*, float, int, CUserCmd*, C_BaseEntity*);
#endif

typedef void (*RandomSeedFn)(int);
typedef float (*RandomFloatFn)(float, float);
typedef float (*RandomFloatExpFn)(float, float, float);
typedef int (*RandomIntFn)(int, int);
typedef float (*RandomGaussianFloatFn)(float, float);

typedef bool (__fastcall* IntersectRayWithBoxFn)(const Ray_t&, const vec3_t&, const vec3_t&, const vec3_t&, trace_t*__restrict);
typedef bool (__thiscall* ClipRayToVPhysicsFn)(IEngineTrace*, const Ray_t&, unsigned int, ICollideable*, studiohdr_t*, trace_t*);
typedef bool (__thiscall* EnumerateElementsAlongRayFn)(CVoxelTree*, unsigned int, const Ray_t&, const vec3&, const vec3&, IEntityEnumerator*);

//MacOS version has some intersection functions inlined yet Windows one uses weird calling conventions for the calls inside them, so we are going to rebuild them only for MacOS. Later on, we might fully rebuild the functions so we do not have platform dependant paths
#ifdef __APPLE__
typedef bool (*IntersectRayWithOBBFn)(const Ray_t&, const vec3&, const vec3&, const vec3&, const vec3&, trace_t*, float);
typedef cmodel_t* (*CM_InlineModelNumberFn)(int);
typedef void (*TransformedBoxTraceFn)(const Ray_t&, int, int, const vec3&, const vec3&, trace_t*);
#else
typedef bool (__thiscall* ClipRayToFn)(IEngineTrace*, const Ray_t&, unsigned int, ICollideable*, trace_t*);
#endif

typedef int (*ThreadIDFn)(void);


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

extern IntersectRayWithBoxFn IntersectRayWithBox;
extern ClipRayToVPhysicsFn ClipRayToVPhysics;

#ifdef __APPLE__
extern IntersectRayWithOBBFn IntersectRayWithOBB;
extern CM_InlineModelNumberFn CM_InlineModelNumber;
extern TransformedBoxTraceFn TransformedBoxTrace;
#else
extern ClipRayToFn ClipRayToBSP;
extern ClipRayToFn ClipRayToOBB;
#endif

extern ThreadIDFn AllocateThreadID;
extern ThreadIDFn FreeThreadID;

struct UpdateData
{
	Players& players;
	Players& prevPlayers;
	const std::vector<int>* updatedPlayers;
	const std::vector<int>* nonUpdatedPlayers;
	bool additionalUpdate;

	UpdateData(Players& p1, Players& p2, const std::vector<int>* uP, const std::vector<int>* nuP, bool b1)
		: players(p1), prevPlayers(p2), updatedPlayers(uP), nonUpdatedPlayers(nuP), additionalUpdate(b1) {}
};


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
	extern int playerCount;
	extern uint64_t playersFl;
	extern LocalPlayer lpData;
	extern C_BasePlayer* localPlayer;
	extern C_BaseCombatWeapon* activeWeapon;
	extern float backtrackCurtime;
	extern int hitboxIDs[];
	extern int reHitboxIDs[];
	extern studiohdr_t* cachedHDRs[];
	extern HistoryList<AimbotTarget, BACKTRACK_TICKS> aimbotTargets;
	extern HistoryList<unsigned int, BACKTRACK_TICKS> aimbotTargetIntersects;
	extern int hitboxToHitbox[];
	extern uint64_t immuneFlags;
	extern int traceCountAvg;
	extern int traceTimeAvg;

	void UpdatePlayers(CUserCmd* cmd);
	void FinishUpdating(UpdateData* data);
	void UpdateLocalData(CUserCmd* cmd, void* hostRunFrameFp);
	void RunFeatures(CUserCmd* cmd, bool* bSendPacket, void* hostRunFrameFp);
	bool IsEnemy(C_BasePlayer* ent);
}

#endif
