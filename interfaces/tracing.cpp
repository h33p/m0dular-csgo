#include "../sdk/framework/interfaces/tracing.h"
#include "../core/fw_bridge.h"

int Tracing::TracePlayers(LocalPlayer* localPlayer, Players* players, vec3_t point, bool skipLocal)
{
	trace_t tr;
	Ray_t ray;
	CTraceFilter filter;

	ray.Init(localPlayer->eyePos, point);
	if (skipLocal)
		filter.pSkip = FwBridge::localPlayer;
	engineTrace->TraceRay(ray, MASK_SHOT, &filter, &tr);

	float distance = ((vec3)localPlayer->eyePos - tr.endpos).Length();

	return (int)distance;
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
			(!prevPlayers || prevID >= prevPlayers->count || ((vec3)players->origin[i / SIMD_COUNT].acc[i % SIMD_COUNT] - (vec3)prevPlayers->origin[prevID / SIMD_COUNT].acc[i % SIMD_COUNT]).LengthSqr() < 4096))
			validPlayer = true; //In CSGO 3D length square is used to check for lagcomp breakage
		else
			backtrackMask[id] |= NON_BACKTRACKABLE;
	}

	return validPlayer;
}
