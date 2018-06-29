#include "fw_bridge.h"
#include "../sdk/framework/utils/threading.h"
#include "../sdk/framework/features/aimbot.h"
#include "../sdk/features/features.h"
#include "engine.h"
#include "hooks.h"
#include <algorithm>

C_BasePlayer* FwBridge::localPlayer = nullptr;
float FwBridge::maxBacktrack = 0;
int FwBridge::hitboxIDs[Hitboxes::HITBOX_MAX];

HistoryList<Players, BACKTRACK_TICKS> FwBridge::playerTrack;
LocalPlayer lpData;

/*
 * We try to be as much cache efficient as possible here.
 * Thus, we split each data entry to it's own separate function,
 * since that way memory read/write will be sequencial on our side.
 * About the game side - not much you can do.
*/

static void UpdateOrigin(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.origin[i] = ent->m_vecOrigin();
		else if (players.flags[i] & Flags::EXISTS)
			players.origin[i] = prevPlayers.origin[i];
	}
}

static void UpdateBoundsStart(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.boundsStart[i / SIMD_COUNT].acc[i % SIMD_COUNT] = ent->m_Collision()->m_vecMins();
		else if (players.flags[i] & Flags::EXISTS)
			players.boundsStart[i / SIMD_COUNT].acc[i % SIMD_COUNT].Set(prevPlayers.boundsStart[i / SIMD_COUNT].acc[i % SIMD_COUNT]);
	}
}

static void UpdateBoundsEnd(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.boundsStart[i / SIMD_COUNT].acc[i % SIMD_COUNT] = ent->m_Collision()->m_vecMins();
		else if (players.flags[i] & Flags::EXISTS)
			players.boundsEnd[i / SIMD_COUNT].acc[i % SIMD_COUNT].Set(prevPlayers.boundsEnd[i / SIMD_COUNT].acc[i % SIMD_COUNT]);
	}
}

__ALIGNED(SIMD_COUNT * 4)
static matrix3x4_t boneMatrix[128];
static mstudiobbox_t* hitboxes[MAX_HITBOXES];
static int boneIDs[MAX_HITBOXES];
static float radius[MAX_HITBOXES];
static float damageMul[MAX_HITBOXES];

static void UpdateHitboxes(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		int hb = -1;
		if (players.flags[i] & Flags::UPDATED) {
			studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());
			if (!hdr)
				continue;

			mstudiohitboxset_t* set = hdr->GetHitboxSet(0);
			if (!set)
				continue;

			if (!Engine::UpdatePlayer(ent, boneMatrix))
				continue;

			for (int idx = 0; idx < set->numhitboxes && hb < MAX_HITBOXES; idx++) {
				if (idx == Hitboxes::HITBOX_UPPER_CHEST ||
						idx == Hitboxes::HITBOX_LEFT_UPPER_ARM || idx == Hitboxes::HITBOX_RIGHT_UPPER_ARM)
					continue;

				hitboxes[++hb] = set->GetHitbox(idx);
				FwBridge::hitboxIDs[idx] = hb;
				if (!hitboxes[hb])
					continue;
				boneIDs[hb] = hitboxes[hb]->bone;
				radius[hb] = hitboxes[hb]->m_flRadius;
				HitGroups hitGroup = (HitGroups)hitboxes[hb]->group;

				bool hasHeavyArmor = ent->m_bHasHeavyArmor();

				float dmgMul = 1.f;

				switch (hitGroup)
				{
				  case HitGroups::HITGROUP_HEAD:
					dmgMul *= hasHeavyArmor ? 2.f : 4.f; //Heavy Armor does 1/2 damage
					break;
				  case HitGroups::HITGROUP_STOMACH:
					dmgMul *= 1.25f;
					break;
				  case HitGroups::HITGROUP_LEFTLEG:
				  case HitGroups::HITGROUP_RIGHTLEG:
					dmgMul *= 0.75f;
					break;
				  default:
					break;
				}

				damageMul[hb] = dmgMul;
			}

			for (; hb < Hitboxes::HITBOX_MAX; hb++)
				FwBridge::hitboxIDs[hb] = -1;

			for (int idx = 0; idx < MAX_HITBOXES; idx++)
				if (hitboxes[idx])
					players.hitboxes[i].start[idx] = hitboxes[idx]->bbmin;

			for (int idx = 0; idx < MAX_HITBOXES; idx++)
				if (hitboxes[idx])
					players.hitboxes[i].end[idx] = hitboxes[idx]->bbmax;

			for (int idx = 0; idx < MAX_HITBOXES; idx++)
				players.hitboxes[i].wm[idx] = boneMatrix[boneIDs[idx]];

			for (int idx = 0; idx < HITBOX_CHUNKS; idx++)
				for (int o = 0; o < SIMD_COUNT; o++)
					players.hitboxes[i].data[idx][0][o] = radius[idx * SIMD_COUNT + o];

			for (int idx = 0; idx < HITBOX_CHUNKS; idx++)
				for (int o = 0; o < SIMD_COUNT; o++)
					players.hitboxes[i].data[idx][1][o] = damageMul[idx * SIMD_COUNT + o];

			players.flags[i] |= Flags::HITBOXES_UPDATED;
		}
		else if (players.flags[i] & Flags::EXISTS)
			players.hitboxes[i] = prevPlayers.hitboxes[i];
	}
}

static void UpdateVelocity(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.velocity[i] = ent->m_vecVelocity();
		else if (players.flags[i] & Flags::EXISTS)
			players.velocity[i] = prevPlayers.velocity[i];
	}
}

static void UpdateHealth(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.health[i] = ent->m_iHealth();
		else if (players.flags[i] & Flags::EXISTS)
			players.health[i] = prevPlayers.health[i];
	}
}

static void UpdateArmor(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.armor[i] = ent->m_ArmorValue();
		else if (players.flags[i] & Flags::EXISTS)
			players.armor[i] = prevPlayers.armor[i];
	}
}

static void SwitchFlags(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		int pID = prevPlayers.sortIDs[players.unsortIDs[i]];
		if (pID < prevPlayers.count && players.flags[i] & Flags::EXISTS && (~players.flags[i]) & Flags::UPDATED && prevPlayers.flags[pID] & Flags::UPDATED) {
			int fl = players.flags[i];
			players.flags[i] = prevPlayers.flags[pID];
			prevPlayers.flags[pID] = fl;
		}
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

struct SortData {
	C_BasePlayer* player;
	float fov;
	int id;
};

static SortData players[64];

//Sort the players for better data layout, in this case - by FOV
static bool PlayerSort(SortData& a, SortData& b)
{
    return a.fov < b.fov;
}

static void UpdateFlags(int& flags, int& cflags, C_BasePlayer* ent)
{
	cflags = Flags::EXISTS;
	if (flags & FL_ONGROUND)
		cflags |= Flags::ONGROUND;
	if (flags & FL_DUCKING)
		cflags |= Flags::DUCKING;
	if (FwBridge::localPlayer && ent->m_iTeamNum() == FwBridge::localPlayer->m_iTeamNum())
		cflags |= Flags::FRIENDLY;
}

void FwBridge::UpdatePlayers(CUserCmd* cmd)
{
	UpdateData data(playerTrack.Push(), playerTrack.GetLastItem(1));
	data.players.count = engine->GetMaxClients();

	int count = 0;

	for (int i = 1; i < 64; i++)
	{
		C_BasePlayer* ent = (C_BasePlayer*)entityList->GetClientEntity(i);

		if (!ent || !ent->IsPlayer() || ent->IsDormant() || i == 0)
			continue;

#ifdef _WIN32
		C_BasePlayer* hookEnt = ent+1;
#else
		C_BasePlayer* hookEnt = ent;
#endif

		if (CSGOHooks::entityHooks->find(hookEnt) == CSGOHooks::entityHooks->end()) {
			VFuncHook* hook = new VFuncHook((void*)hookEnt);
#ifndef _WIN32
			hook->Hook(1, CSGOHooks::EntityDestruct);
#endif
			hook->Hook(PosixWin(104, 52), CSGOHooks::SetupBones);
			CSGOHooks::entityHooks->insert({hookEnt, hook});
		}

		if (ent == localPlayer) {
			lpData.ID = i;
			continue;
		}

		vec3_t angle = ((vec3_t)ent->m_vecOrigin() - lpData.eyePos).GetAngles(true);
		vec3_t angleDiff = (lpData.angles - angle).NormalizeAngles<2>(-180.f, 180.f);

		players[count].fov = angleDiff.Length<2>();
		players[count].player = ent;
		players[count].id = i;
		count++;
	}

	std::sort(players, players + count, PlayerSort);

	for (int i = 0; i < count; i++) {

		C_BasePlayer* ent = players[i].player;

		data.players.instance[i] = (void*)ent;
		data.players.fov[i] = players[i].fov;
		data.players.sortIDs[players[i].id] = i;
		data.players.unsortIDs[i] = players[i].id;

		data.players.time[i] = ent->m_flSimulationTime();

		int flags = ent->m_fFlags();
		int cflags;
		UpdateFlags(flags, cflags, ent);

		if (data.players.time[i] != data.prevPlayers.time[data.prevPlayers.sortIDs[data.players.unsortIDs[i]]] && ent->m_lifeState() == LIFE_ALIVE)
			cflags |= Flags::UPDATED;

		data.players.flags[i] = cflags;
	}
	data.players.count = count;

	Engine::StartLagCompensation();

	//We want to push empty lists only if the previous list was not empty.
	if (count > 0 || data.prevPlayers.count > 0) {
		//Updating the hitboxes calls engine functions that only work on the main thread
		//While it is being done, let's update other data on a seperate thread
		//Flags depend on the animation fix fixing up the player.
		UpdateOrigin(data.players, data.prevPlayers);
		Engine::StartAnimationFix(&data.players, &data.prevPlayers);
		Threading::QueueJobRef(ThreadedUpdate, &data);
		UpdateHitboxes(data.players, data.prevPlayers);
		Engine::EndAnimationFix(&data.players, &data.prevPlayers);
		Threading::FinishQueue();
		SwitchFlags(data.players, data.prevPlayers);
	} else
		playerTrack.UndoPush();

}

void RunSimulation(CPrediction* prediction, float curtime, int command_number, CUserCmd* tCmd, C_BaseEntity* localPlayer)
{
#ifdef _WIN32
	RunSimulationFunc(prediction, nullptr, 0, 0, curtime, command_number, tCmd, localPlayer);
#else
	RunSimulationFunc(prediction, curtime, command_number, tCmd, localPlayer);
#endif
}

void FwBridge::UpdateLocalData(CUserCmd* cmd, void* hostRunFrameFp)
{
	localPlayer = (C_BasePlayer*)entityList->GetClientEntity(engine->GetLocalPlayer());
	C_BaseCombatWeapon* activeWeapon = localPlayer->m_hActiveWeapon();

	if (activeWeapon) {
		lpData.weaponAmmo = activeWeapon->m_iClip1();
		CCSWeaponInfo* weaponInfo = GetWeaponInfo(weaponDatabase, activeWeapon->m_iItemDefinitionIndex());
		if (weaponInfo) {
			lpData.weaponArmorPenetration = weaponInfo->flArmorRatio();
			lpData.weaponRange = weaponInfo->flRange();
			lpData.weaponRangeModifier = weaponInfo->flRangeModifier();
			lpData.weaponPenetration = weaponInfo->flPenetration();
			lpData.weaponDamage = weaponInfo->iDamage();
		} else {
			lpData.weaponPenetration = 0;
			lpData.weaponRange = 0;
			lpData.weaponRangeModifier = 0;
			lpData.weaponPenetration = 0;
			lpData.weaponDamage = 0;
		}
	} else {
		lpData.weaponAmmo = 0;
	}

	SourceEnginePred::Prepare(cmd, localPlayer, hostRunFrameFp);
	SourceEnginePred::Run(cmd, localPlayer);

	lpData.eyePos = Weapon_ShootPosition(localPlayer);

	float recoilScale = 1.f;

	static ConVar* weapon_recoil_scale = cvar->FindVar("weapon_recoil_scale");

	if (weapon_recoil_scale)
		recoilScale = weapon_recoil_scale->GetFloat();

	lpData.aimOffset = localPlayer->m_aimPunchAngle() * recoilScale;

	int flags = localPlayer->m_fFlags();
	int cflags;
	UpdateFlags(flags, cflags, localPlayer);
	lpData.flags = cflags;

	SourceEssentials::UpdateData(cmd, &lpData);
}

void FwBridge::RunFeatures(CUserCmd* cmd, bool* bSendPacket)
{
	maxBacktrack = Engine::CalculateBacktrackTime();

	SourceBhop::Run(cmd, &lpData);
	//Aimbot part
	if (true || lpData.keys & Keys::ATTACK1) {
		Target target = Aimbot::RunAimbot(&playerTrack, &lpData, maxBacktrack);
		//Disable the actual aimbot for now
		//lpData.angles = cmd->viewangles;

		if (target.id >= 0) {
			cmd->tick_count = TimeToTicks(playerTrack.GetLastItem(target.backTick).time[target.id] + Engine::LerpTime());
			cmd->buttons |= IN_ATTACK;
		}
	}

	SourceEssentials::UpdateCMD(cmd, &lpData);
	SourceEnginePred::Finish(cmd, localPlayer);
	Engine::EndLagCompensation();
}
