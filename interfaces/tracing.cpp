#include "../sdk/framework/interfaces/tracing.h"
#include "../core/fw_bridge.h"
#include "../core/engine.h"

int Tracing::TracePlayers(LocalPlayer* localPlayer, Players* players, vec3_t point, int eID, int depth, bool skipLocal)
{
	trace_t tr;
	Ray_t ray;
	CTraceFilter filter;

	ray.Init(localPlayer->eyePos, point);
	if (skipLocal)
		filter.pSkip = FwBridge::localPlayer;
	engineTrace->TraceRay(ray, MASK_SHOT, &filter, &tr);

	if ((void*)tr.m_pEnt != players->instance[eID])
		return -1.f;

	int hbID = FwBridge::hitboxIDs[tr.hitbox];

	if (hbID < 0)
		return -1.f;

	float distance = ((vec3)localPlayer->eyePos - tr.endpos).Length();
	float damage = localPlayer->weaponDamage * powf(localPlayer->weaponRangeModifier, distance * 0.002f);

	return (int)(damage * players->hitboxes[eID].data[hbID / SIMD_COUNT][hbID % SIMD_COUNT][1]);
}

enum BTMask
{
	NON_BACKTRACKABLE = (1 << 0)
};

bool Tracing::BacktrackPlayers(Players* players, Players* prevPlayers, char backtrackMask[MAX_PLAYERS])
{
	int count = players->count;
	for (int i = 0; i < count; i++)
		if (players->flags[i] & Flags::HITBOXES_UPDATED && players->time[i] < FwBridge::maxBacktrack)
			return false;

	bool validPlayer = false;

	for (int i = 0; i < count; i++) {
		int id = players->unsortIDs[i];
		int prevID = prevPlayers ? prevPlayers->sortIDs[id] : MAX_PLAYERS;
		if (players->flags[i] & Flags::HITBOXES_UPDATED &&
			~backtrackMask[id] & BTMask::NON_BACKTRACKABLE &&
			(!prevPlayers || prevID >= prevPlayers->count || ((vec3)players->origin[i] - (vec3)prevPlayers->origin[prevID]).LengthSqr() < 4096))
			validPlayer = true; //In CSGO 3D length square is used to check for lagcomp breakage
		else
			backtrackMask[id] |= NON_BACKTRACKABLE;
	}

	if (validPlayer) {
		for (int i = 0; i < count; i++) {
			int id = players->unsortIDs[i];
			if (players->flags[i] & Flags::HITBOXES_UPDATED && ~backtrackMask[id] & BTMask::NON_BACKTRACKABLE) {
				C_BasePlayer* ent = (C_BasePlayer*)players->instance[i];
				vec3 origin = (vec3)players->origin[i];
				SetAbsOrigin(ent, origin);
				CUtlVector<matrix3x4_t>& matrix = ent->m_nBoneMatrix();
				int bones = Engine::numBones[id];

				memcpy(matrix.m_Memory.m_pMemory, players->hitboxes[i].wm, sizeof(matrix3x4_t) * bones);
			}
		}
	}

	return validPlayer;
}
