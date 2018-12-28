#include "../sdk/framework/interfaces/tracing.h"
#include "../core/tracing.h"
#include "../core/fw_bridge.h"
#include "../core/engine.h"
#include "../core/spread.h"
#include "../sdk/framework/utils/intersect_impl.h"
#include "../sdk/framework/utils/threading.h"
#include "../core/hooks.h"

int Tracing::TracePlayer(LocalPlayer* localPlayer, Players* players, vec3_t point, int eID, int depth, bool skipLocal)
{
	trace_t tr;
	Tracing2::TracePart1(localPlayer->eyePos, point, &tr, skipLocal ? FwBridge::localPlayer : nullptr);
	return Tracing2::TracePart2(localPlayer->eyePos, localPlayer->weaponDamage, localPlayer->weaponRangeModifier, players, &tr, eID);
}

template<size_t N>
void Tracing::TracePlayerSIMD(LocalPlayer* localPlayer, Players* players, vec3soa<float, N> point, int eID, int out[N], int depth, bool skipLocal)
{
	trace_t tr[N];
	C_BasePlayer* skipEnt = skipLocal ? FwBridge::localPlayer : nullptr;
	for (size_t i = 0; i < N; i++)
		Tracing2::TracePart1(localPlayer->eyePos, (vec3_t)point.acc[i], tr + i, skipEnt);
	for (size_t i = 0; i < N; i++)
		out[i] = Tracing2::TracePart2(localPlayer->eyePos, localPlayer->weaponDamage, localPlayer->weaponRangeModifier, players, tr + i, eID);
}

thread_local std::vector<trace_t> traces;
thread_local std::vector<Ray_t> rays;

void Tracing::TracePointList(LocalPlayer* localPlayer, Players* players, size_t n, const vec3_t* points, int eID, int* __restrict out, int depth, bool skipLocal)
{
	if (!n)
		return;

	traces.resize(n);
	rays.resize(n);

	for (size_t i = 0; i < n; i++)
		rays[i].Init(localPlayer->eyePos, points[i]);

	//Perform autowall
	if (depth == 1) {

		for (size_t i = 0; i < n && !Tracing2::TraceBudgetEmpty(); i++) {
			int cacheSize = 0;
			bool permaCache[AutoWall::MAX_INTERSECTS * 2 + 2];
			trace_t outTraces[AutoWall::MAX_INTERSECTS * 2 + 2];
			float outDamages[AutoWall::MAX_INTERSECTS * 2 + 2];
		    AutoWall::FireBulletWorld(localPlayer->eyePos, rays[i].delta.Normalized(), localPlayer->weaponRange, localPlayer->weaponRangeModifier, localPlayer->weaponDamage, localPlayer->weaponPenetration, &cacheSize, permaCache, outTraces, outDamages);
		    *out = AutoWall::FireBulletPlayers(localPlayer->eyePos, rays[i].delta.Normalized(), localPlayer->weaponRange, localPlayer->weaponRangeModifier, localPlayer->weaponDamage, localPlayer->weaponPenetration, localPlayer->weaponArmorPenetration, &cacheSize, permaCache, outTraces, outDamages, players);
		}

		return;
	}

	CTraceFilterSkipPlayers filter;

	Tracing2::TraceRayTargetOptimized(n, traces.data(), rays.data(), MASK_SHOT, &filter, eID, players);


	for (size_t i = 0; i < n; i++)
		Tracing2::ClipTraceToPlayers(&traces[i], players, 0);

	for (size_t i = 0; i < n; i++)
		out[i] = Tracing2::TracePart2(localPlayer->eyePos, localPlayer->weaponDamage, localPlayer->weaponRangeModifier, players, &traces[i], eID);
}

thread_local std::vector<vec3_t> pointsV3;

template<size_t N>
void Tracing::TracePointListSIMD(LocalPlayer* localPlayer, Players* players, size_t n, const vec3soa<float, N>* points, int eID, int* __restrict out, int depth, bool skipLocal)
{
	if (!n)
		return;

	//We do not support SIMD optimized functions for tracing (would only be useful if the whole source tracer was rebuilt) so simply pass rotated point array

	pointsV3.clear();

	for (size_t u = 0; u < n; u++)
		for (size_t i = 0; i < N; i++)
			pointsV3.push_back((vec3_t)points[u].acc[i]);

	TracePointList(localPlayer, players, n * N, pointsV3.data(), eID, out, depth, skipLocal);
}

//Template size definitions to make the linking successful
template void Tracing::TracePlayerSIMD<MULTIPOINT_COUNT>(LocalPlayer* localPlayer, Players* players, mvec3 point, int eID, int out[MULTIPOINT_COUNT], int depth, bool skipLocal);
template void Tracing::TracePointListSIMD<MULTIPOINT_COUNT>(LocalPlayer* localPlayer, Players* players, size_t n, const mvec3* points, int eID, int* __restrict out, int depth, bool skipLocal);
//template void Tracing::TracePlayersSIMD<SIMD_COUNT>(LocalPlayer* localPlayer, Players* players, nvec3 point, int eID, int out[SIMD_COUNT], int depth, bool skipLocal);

enum BTMask
{
	NON_BACKTRACKABLE = (1 << 0),
	FIRST_TIME_DONE = (1 << 1),
	BREAKING_LC = (1 << 2)
};

static vec3_t prevOrigin[MAX_PLAYERS];
static ConVar* cl_lagcompensation = nullptr;

bool Tracing::BacktrackPlayers(Players* players, Players* prevPlayers, char backtrackMask[MAX_PLAYERS])
{
	if (!cl_lagcompensation)
		cl_lagcompensation = cvar->FindVar(ST("cl_lagcompensation"));

	bool lcBreak = cl_lagcompensation && !cl_lagcompensation->GetBool();

	int count = players->count;

	uint64_t validPlayers = 0;

	for (int i = 0; i < count; i++) {
		if (~players->flags[i] & Flags::HITBOXES_UPDATED || !(lcBreak || fabsf(players->time[i] - FwBridge::backtrackCurtime) > 0.2f) || ~backtrackMask[players->unsortIDs[i]] & FIRST_TIME_DONE)
			validPlayers |= 1ull << i;
	}


	bool validPlayer = false;

	for (int i = 0; i < count; i++) {
		int id = players->unsortIDs[i];
		float distDelta = (players->origin[i] - prevOrigin[id]).LengthSqr();
		if (validPlayers & (1ull << i) &&
			players->flags[i] & Flags::HITBOXES_UPDATED &&
			FwBridge::playersFl & (1ull << id) &&
			~backtrackMask[id] & BTMask::NON_BACKTRACKABLE &&
			(~backtrackMask[id] & FIRST_TIME_DONE || distDelta < 4096.f)) {
			validPlayer = true; //In CSGO 3D length square is used to check for lagcomp breakage
			backtrackMask[id] |= FIRST_TIME_DONE;
			prevOrigin[id] = players->origin[i];
		} else {
			backtrackMask[id] |= NON_BACKTRACKABLE;
			if (distDelta >= 4096.f)
				backtrackMask[id] |= BREAKING_LC;
		}
	}

	return validPlayer;
}

bool Tracing::VerifyTarget(Players* players, int id, char backtrackMask[MAX_PLAYERS])
{
	bool lcBreak = cl_lagcompensation && !cl_lagcompensation->GetBool();
	float timeDelta = fabsf(players->time[id] - FwBridge::backtrackCurtime);
	//cvar->ConsoleDPrintf("%d %f\n", id, timeDelta);
	return (backtrackMask[id] & BREAKING_LC) || lcBreak || timeDelta <= 0.2f;
}
