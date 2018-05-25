#include "fw_bridge.h"

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
			players.origin[i / SIMD_COUNT].acc[i % SIMD_COUNT].Set(prevPlayers.origin[i / SIMD_COUNT].acc[i % SIMD_COUNT]);
		else if (players.flags[i] & Flags::EXISTS)
			players.origin[i / SIMD_COUNT].acc[i % SIMD_COUNT] = ent->m_vecOrigin();
	}
}

static void UpdateBoundsStart(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count && i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.boundsStart[i / SIMD_COUNT].acc[i % SIMD_COUNT].Set(prevPlayers.boundsStart[i / SIMD_COUNT].acc[i % SIMD_COUNT]);
		else if (players.flags[i] & Flags::EXISTS);

	}
}

static void UpdateBoundsEnd(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count && i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.boundsEnd[i / SIMD_COUNT].acc[i % SIMD_COUNT].Set(prevPlayers.boundsEnd[i / SIMD_COUNT].acc[i % SIMD_COUNT]);
		else if (players.flags[i] & Flags::EXISTS);

	}
}

static void UpdateHitboxes(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count && i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.hitboxes[i] = prevPlayers.hitboxes[i];
		else if (players.flags[i] & Flags::EXISTS);

	}
}

static void UpdateVelocity(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count && i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.velocity[i] = prevPlayers.velocity[i];
		else if (players.flags[i] & Flags::EXISTS)
			players.velocity[i] = ent->m_vecVelocity();
	}
}

static void UpdateHealth(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count && i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.health[i] = prevPlayers.health[i];
		else if (players.flags[i] & Flags::EXISTS)
			players.health[i] = ent->m_iHealth();
	}
}

static void UpdateArmor(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count && i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.armor[i] = prevPlayers.armor[i];
		else if (players.flags[i] & Flags::EXISTS)
			players.armor[i] = ent->m_ArmorValue();
	}
}

void FwBridge::UpdatePlayers(CUserCmd* cmd)
{
	Players players;
	players.count = engine->GetMaxClients();
	Players& prevPlayers = playerTrack.GetLastItem(0);
	bool updatedOnce = false;

	for (int i = 0; i < players.count && i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)entityList->GetClientEntity(i);
		if (ent == localPlayer || !ent || !ent->IsPlayer()) {
			players.flags[i] = 0;
			continue;
		}
		players.instance[i] = (void*)ent;
		players.time[i] = ent->m_flSimulationTime();
		if (players.time[i] == prevPlayers.time[i])
			players.flags[i] = prevPlayers.flags[i] & ~Flags::UPDATED;
		else {
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

	//We don't want to push a completely same list as before
	if (updatedOnce) {
		UpdateOrigin(players, prevPlayers);
		UpdateBoundsStart(players, prevPlayers);
		UpdateBoundsEnd(players, prevPlayers);
		UpdateHitboxes(players, prevPlayers);
		UpdateVelocity(players, prevPlayers);
		UpdateHealth(players, prevPlayers);
		UpdateArmor(players, prevPlayers);
		playerTrack.Push(players);
	}
}

void FwBridge::UpdateLocalData(CUserCmd* cmd)
{
	localPlayer = (C_BaseEntity*)entityList->GetClientEntity(engine->GetLocalPlayer());
}
