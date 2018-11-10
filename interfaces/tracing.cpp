#include "../sdk/framework/interfaces/tracing.h"
#include "../core/tracing.h"
#include "../core/fw_bridge.h"
#include "../core/engine.h"
#include "../core/spread.h"
#include "../sdk/framework/utils/intersect_impl.h"

int Tracing2::traceCounter = 0;

static void TracePart1(vec3_t eyePos, vec3_t point, trace_t* tr, C_BasePlayer* skipEnt)
{
	Ray_t ray;
	CTraceFilter filter;

	Tracing2::traceCounter++;

	ray.Init(eyePos, point);
	filter.pSkip = skipEnt;

	memset(tr, 0, sizeof(trace_t));

	engineTrace->TraceRay(ray, MASK_SHOT, &filter, tr);
}

static float TracePart2(vec3_t eyePos, float weaponDamage, float weaponRangeModifier, Players* players, trace_t* tr, int eID)
{
	if (eID < 0 && tr->ent != FwBridge::localPlayer)
		return -1.f;

	//TODO: add target entity intersection test here in case we are not directly aiming at entity
	if (eID >= 0 && tr->ent && (void*)tr->ent != players->instance[eID])
		return -1.f;

	if (eID >= 0) {
		unsigned int flag = 0;
		for (size_t i = 0; i < NumOfSIMD(MAX_HITBOXES); i++)
			flag |= players->colliders[eID][i].Intersect(eyePos, tr->endpos);

		if (!flag)
			return -1.f;
	}


	int hbID = FwBridge::hitboxIDs[tr->hitbox];

	if (hbID < 0)
		return -1.f;

	float distance = ((vec3)eyePos - tr->endpos).Length();
	float damage = weaponDamage * powf(weaponRangeModifier, distance * 0.002f);

	//HACK HACK HACK this is very unsafe, we just assume there will be a player
	if (eID < 0)
		return damage * players->hitboxes[0].damageMul[hbID];
	return (int)(damage * players->hitboxes[eID].damageMul[hbID]);
}

int Tracing2::TracePlayers(vec3_t eyePos, float weaponDamage, float weaponRangeModifier, Players* players, vec3_t point, int eID, int depth, C_BasePlayer* skipEnt)
{
	trace_t tr;
	TracePart1(eyePos, point, &tr, skipEnt);
	return TracePart2(eyePos, weaponDamage, weaponRangeModifier, players, &tr, eID);
}

template<size_t N>
void Tracing2::TracePlayersSIMD(vec3_t eyePos, float weaponDamage, float weaponRangeModifier, Players* players, vec3soa<float, N> point, int eID, int out[N], int depth, C_BasePlayer* skipEnt)
{
	trace_t tr[N];
	for (size_t i = 0; i < N; i++)
		TracePart1(eyePos, (vec3_t)point.acc[i], tr + i, skipEnt);
	for (size_t i = 0; i < N; i++)
		out[i] = TracePart2(eyePos, weaponDamage, weaponRangeModifier, players, tr + i, eID);
}

template void Tracing2::TracePlayersSIMD<MULTIPOINT_COUNT>(vec3_t eyePos, float weaponDamage, float weaponRangeModifier, Players* players, mvec3 point, int eID, int out[MULTIPOINT_COUNT], int depth, C_BasePlayer* skipEnt);

int Tracing::TracePlayers(LocalPlayer* localPlayer, Players* players, vec3_t point, int eID, int depth, bool skipLocal)
{
	trace_t tr;
	TracePart1(localPlayer->eyePos, point, &tr, skipLocal ? FwBridge::localPlayer : nullptr);
	return TracePart2(localPlayer->eyePos, localPlayer->weaponDamage, localPlayer->weaponRangeModifier, players, &tr, eID);
}

template<size_t N>
void Tracing::TracePlayersSIMD(LocalPlayer* localPlayer, Players* players, vec3soa<float, N> point, int eID, int out[N], int depth, bool skipLocal)
{
	trace_t tr[N];
	C_BasePlayer* skipEnt = skipLocal ? FwBridge::localPlayer : nullptr;
	for (size_t i = 0; i < N; i++)
		TracePart1(localPlayer->eyePos, (vec3_t)point.acc[i], tr + i, skipEnt);
	for (size_t i = 0; i < N; i++)
		out[i] = TracePart2(localPlayer->eyePos, localPlayer->weaponDamage, localPlayer->weaponRangeModifier, players, tr + i, eID);
}

//Template size definitions to make the linking successful
template void Tracing::TracePlayersSIMD<MULTIPOINT_COUNT>(LocalPlayer* localPlayer, Players* players, mvec3 point, int eID, int out[MULTIPOINT_COUNT], int depth, bool skipLocal);
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

	for (int i = 0; i < count; i++)
		if (players->flags[i] & Flags::HITBOXES_UPDATED && (lcBreak || fabsf(players->time[i] - FwBridge::backtrackCurtime) > 0.2f) && backtrackMask[players->unsortIDs[i]] & FIRST_TIME_DONE)
			return false;

	bool validPlayer = false;

	for (int i = 0; i < count; i++) {
		int id = players->unsortIDs[i];
		float distDelta = (players->origin[i] - prevOrigin[id]).LengthSqr();
		if (players->flags[i] & Flags::HITBOXES_UPDATED &&
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

	if (validPlayer) {
		for (int i = 0; i < count; i++) {
			int id = players->unsortIDs[i];
			if (players->flags[i] & Flags::HITBOXES_UPDATED && FwBridge::playersFl & (1ull << id) && ~backtrackMask[id] & BTMask::NON_BACKTRACKABLE) {
				C_BasePlayer* ent = (C_BasePlayer*)players->instance[i];
				vec3 origin = (vec3)players->origin[i];
				SetAbsOrigin(ent, origin);
				CUtlVector<matrix3x4_t>& matrix = ent->boneMatrix();
				int bones = Engine::numBones[id];

				memcpy(*(void**)&matrix, players->bones[i], sizeof(matrix3x4_t) * bones);
			}
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
