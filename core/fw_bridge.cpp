#include "fw_bridge.h"
#include "../framework/utils/threading.h"

C_BaseEntity* FwBridge::localPlayer = nullptr;

HistoryList<Players, BACKTRACK_TICKS> FwBridge::playerTrack;

/*
 * We try to be as much cache efficient as possible here.
 * Thus, we split each data entry to it's own separate function,
 * since that way memory read/write will be sequencial on our side.
 * About the game side - not much you can do.
*/

static void UpdateOrigin(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count && i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.origin[i / SIMD_COUNT].acc[i % SIMD_COUNT] = ent->m_vecOrigin();
		else if (players.flags[i] & Flags::EXISTS)
			players.origin[i / SIMD_COUNT].acc[i % SIMD_COUNT].Set(prevPlayers.origin[i / SIMD_COUNT].acc[i % SIMD_COUNT]);
	}
}

static void UpdateBoundsStart(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count && i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED);
		else if (players.flags[i] & Flags::EXISTS)
			players.boundsStart[i / SIMD_COUNT].acc[i % SIMD_COUNT].Set(prevPlayers.boundsStart[i / SIMD_COUNT].acc[i % SIMD_COUNT]);

	}
}

static void UpdateBoundsEnd(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count && i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED);
		else if (players.flags[i] & Flags::EXISTS)
			players.boundsEnd[i / SIMD_COUNT].acc[i % SIMD_COUNT].Set(prevPlayers.boundsEnd[i / SIMD_COUNT].acc[i % SIMD_COUNT]);

	}
}

static void UpdateHitboxes(Players& __restrict players, Players& __restrict prevPlayers)
{
	matrix3x4_t boneMatrix[128];
	for (int i = 0; i < players.count && i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED) {
			studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());
			if (!hdr)
				continue;
			mstudiohitboxset_t* set = hdr->GetHitboxSet(0);
			if (!set)
				continue;
			if (!ent->SetupBones(boneMatrix, MAXSTUDIOBONES, BONE_USED_BY_HITBOX, globalVars->curtime))
				continue;
		}
		else if (players.flags[i] & Flags::EXISTS)
			players.hitboxes[i] = prevPlayers.hitboxes[i];
	}
}

static void UpdateVelocity(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count && i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.velocity[i] = ent->m_vecVelocity();
		else if (players.flags[i] & Flags::EXISTS)
			players.velocity[i] = prevPlayers.velocity[i];
	}
}

static void UpdateHealth(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count && i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.health[i] = ent->m_iHealth();
		else if (players.flags[i] & Flags::EXISTS)
			players.health[i] = prevPlayers.health[i];
	}
}

static void UpdateArmor(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count && i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.armor[i] = ent->m_ArmorValue();
		else if (players.flags[i] & Flags::EXISTS)
			players.armor[i] = prevPlayers.armor[i];
	}
}

struct UpdateData
{
	Players& players;
	Players& prevPlayers;

	UpdateData(Players& p1, Players& p2) : players(p1), prevPlayers(p2) {}
};

static void ThreadedUpdate(UpdateData* data)
{
		UpdateBoundsStart(data->players, data->prevPlayers);
		UpdateBoundsEnd(data->players, data->prevPlayers);
		UpdateVelocity(data->players, data->prevPlayers);
		UpdateHealth(data->players, data->prevPlayers);
		UpdateArmor(data->players, data->prevPlayers);
}

void FwBridge::UpdatePlayers(CUserCmd* cmd)
{
	Players players;
	players.count = engine->GetMaxClients();
	UpdateData data(players, playerTrack.GetLastItem(0));
	bool updatedOnce = false;

	int max = 0;
	for (int i = 0; i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)entityList->GetClientEntity(i);
		if (ent == localPlayer || !ent || !ent->IsPlayer()) {
			players.flags[i] = 0;
			continue;
		}
		players.instance[i] = (void*)ent;
		players.time[i] = ent->m_flSimulationTime();
		if (players.time[i] == data.prevPlayers.time[i])
			players.flags[i] = data.prevPlayers.flags[i] & ~Flags::UPDATED;
		else {
			max = i + 1;
			int flags = ent->m_fFlags();
			int cflags = Flags::EXISTS | Flags::UPDATED;
			if (flags & FL_ONGROUND)
				cflags |= Flags::ONGROUND;
			if (flags & FL_DUCKING)
				cflags |= Flags::DUCKING;
			players.flags[i] = cflags;
			updatedOnce = true;
		}
	}
	players.count = max;

	//We don't want to push a completely same list as before
	if (updatedOnce) {
		//Updating the hitboxes calls engine functions that only work on the main thread
		//While it is being done, let's update other data on a seperate thread
		Threading::QueueJobRef(ThreadedUpdate, &data);
		UpdateOrigin(data.players, data.prevPlayers);
		UpdateHitboxes(players, data.prevPlayers);
		Threading::FinishQueue();
		playerTrack.Push(players);
	}
}

void FwBridge::UpdateLocalData(CUserCmd* cmd)
{
	localPlayer = (C_BaseEntity*)entityList->GetClientEntity(engine->GetLocalPlayer());
}
