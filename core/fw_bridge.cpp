#include "fw_bridge.h"

#include "../sdk/framework/utils/threading.h"
#include "../sdk/framework/utils/stackstring.h"
#include "../sdk/features/features.h"
#include "../sdk/framework/utils/intersect_impl.h"

#include "spread.h"
#include "engine.h"
#include "hooks.h"
#include "visuals.h"
#include "impacts.h"
#include "antiaim.h"

#include <algorithm>


C_BasePlayer* FwBridge::localPlayer = nullptr;
C_BaseCombatWeapon* FwBridge::activeWeapon = nullptr;
float FwBridge::maxBacktrack = 0;
int FwBridge::hitboxIDs[Hitboxes::HITBOX_MAX];
HistoryList<Players, BACKTRACK_TICKS> FwBridge::playerTrack;
LocalPlayer FwBridge::lpData;
HistoryList<Target, BACKTRACK_TICKS> FwBridge::aimbotTargets;


static ConVar* weapon_recoil_scale = nullptr;

struct SortData {
	C_BasePlayer* player;
	float fov;
	int id;
};

static SortData players[64];

struct UpdateData
{
	Players& players;
	Players& prevPlayers;

	UpdateData(Players& p1, Players& p2) : players(p1), prevPlayers(p2) {}
};


static void ExecuteAimbot(CUserCmd* cmd, bool* bSendPacket, FakelagState state);
static void ThreadedUpdate(UpdateData* data);
static bool PlayerSort(SortData& a, SortData& b);
static void UpdateFlags(int& flags, int& cflags, C_BasePlayer* ent);

/*
  We try to be as much cache efficient as possible here.
  Thus, we split each data entry to it's own separate function,
  since that way memory read/write will be sequencial on our side.
  About the game side - not much you can do.
*/
static void UpdateOrigin(Players& __restrict players, Players& __restrict prevPlayers);
static void UpdateBoundsStart(Players& __restrict players, Players& __restrict prevPlayers);
static void UpdateBoundsEnd(Players& __restrict players, Players& __restrict prevPlayers);
static void UpdateVelocity(Players& __restrict players, Players& __restrict prevPlayers);
static void UpdateHealth(Players& __restrict players, Players& __restrict prevPlayers);
static void UpdateArmor(Players& __restrict players, Players& __restrict prevPlayers);
static void SwitchFlags(Players& __restrict players, Players& __restrict prevPlayers);
static void UpdateHitboxes(Players& __restrict players, Players& __restrict prevPlayers);
static void UpdateColliders(Players& __restrict players, Players& __restrict prevPlayers);


void FwBridge::UpdateLocalData(CUserCmd* cmd, void* hostRunFrameFp)
{
	localPlayer = (C_BasePlayer*)entityList->GetClientEntity(engine->GetLocalPlayer());
	activeWeapon = localPlayer->activeWeapon();

	if (activeWeapon) {
		lpData.weaponAmmo = activeWeapon->clip1();
		CCSWeaponInfo* weaponInfo = GetWeaponInfo(weaponDatabase, activeWeapon->itemDefinitionIndex());
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
	lpData.velocity = localPlayer->velocity();
	lpData.origin = localPlayer->origin();
	lpData.time = globalVars->interval_per_tick * localPlayer->tickBase();

	float recoilScale = 1.f;

	if (!weapon_recoil_scale)
		weapon_recoil_scale = cvar->FindVar(StackString("weapon_recoil_scale"));

	if (weapon_recoil_scale)
		recoilScale = weapon_recoil_scale->GetFloat();

	lpData.aimOffset = localPlayer->aimPunchAngle() * recoilScale;

	int flags = localPlayer->flags();
	int cflags = 0;
	UpdateFlags(flags, cflags, localPlayer);
	lpData.flags = cflags;

	SourceEssentials::UpdateData(cmd, &lpData);
}

void FwBridge::UpdatePlayers(CUserCmd* cmd)
{
	UpdateData data(playerTrack.Push(), playerTrack.GetLastItem(1));
	data.players.count = engine->GetMaxClients();

	int count = 0;

	for (int i = 0; i < MAX_PLAYERS; i++)
		data.players.sortIDs[i] = MAX_PLAYERS;

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

		if (false && CSGOHooks::entityHooks->find(hookEnt) == CSGOHooks::entityHooks->end()) {
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

		vec3_t angle = ((vec3_t)ent->origin() - lpData.eyePos).GetAngles(true);
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

		data.players.time[i] = ent->simulationTime();

		int flags = ent->flags();
		int cflags;
		UpdateFlags(flags, cflags, ent);

		if (data.players.time[i] != data.prevPlayers.time[data.prevPlayers.sortIDs[data.players.unsortIDs[i]]] && ent->lifeState() == LIFE_ALIVE)
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
		Impacts::Tick();
		Engine::StartAnimationFix(&data.players, &data.prevPlayers);
		Threading::QueueJobRef(ThreadedUpdate, &data);
		UpdateHitboxes(data.players, data.prevPlayers);
		UpdateColliders(data.players, data.prevPlayers);
		Threading::FinishQueue();
		SwitchFlags(data.players, data.prevPlayers);
	} else
		playerTrack.UndoPush();
}

void FwBridge::RunFeatures(CUserCmd* cmd, bool* bSendPacket, void* hostRunFrameFp)
{
	maxBacktrack = Engine::CalculateBacktrackTime();

	SourceBhop::Run(cmd, &lpData);
	SourceAutostrafer::Run(cmd, &lpData);
	FakelagState state = SourceFakelag::Run(cmd, &lpData, bSendPacket, !*((long*)hostRunFrameFp - RUNFRAME_TICK));
	ExecuteAimbot(cmd, bSendPacket, state);

	Antiaim::Run(cmd, state);
	SourceEssentials::UpdateCMD(cmd, &lpData);
	SourceEnginePred::Finish(cmd, localPlayer);
	Engine::EndLagCompensation();

	Visuals::shouldDraw = true;
}


//This way it is way cleaner
using namespace FwBridge;

static bool allowShoot = false;
float lastPrimary = 0.f;

static void ExecuteAimbot(CUserCmd* cmd, bool* bSendPacket, FakelagState state)
{
	//Aimbot part
	Target target;
	target.id = -1;

	//We can only shoot once until we take a shot
	if (lastPrimary != activeWeapon->nextPrimaryAttack())
		allowShoot = false;
	lastPrimary = activeWeapon->nextPrimaryAttack();
	if (!allowShoot)
		allowShoot = (state == FakelagState::REAL);
	if (!allowShoot)
		lpData.keys &= ~Keys::ATTACK1;

	if (allowShoot && activeWeapon && activeWeapon->nextPrimaryAttack() <= globalVars->curtime && (true || lpData.keys & Keys::ATTACK1)) {
		bool hitboxList[MAX_HITBOXES];
		memset(hitboxList, 0xff, sizeof(hitboxList));

		target = Aimbot::RunAimbot(&playerTrack, &lpData, maxBacktrack, hitboxList);
		//Disable the actual aimbot for now
		//lpData.angles = cmd->viewangles;

		if (target.id >= 0) {
			cmd->tick_count = TimeToTicks(playerTrack.GetLastItem(target.backTick).time[target.id] + Engine::LerpTime());
			if (false && !Spread::HitChance(&playerTrack.GetLastItem(target.backTick), target.id, target.targetVec, target.boneID))
				lpData.keys &= ~Keys::ATTACK1;
		} else
			lpData.angles -= lpData.aimOffset;
		Spread::CompensateSpread(cmd);
	}

	if (target.id >= 0) {
		vec3_t dir, up, down;
		(lpData.angles + lpData.aimOffset).GetVectors(dir, up, down, true);
		vec3_t endPoint = dir * lpData.weaponRange + lpData.eyePos;

		CapsuleColliderSOA<SIMD_COUNT>* colliders = playerTrack.GetLastItem(target.backTick).colliders[target.id];

		unsigned int flags = 0;

		if (lpData.keys & Keys::ATTACK1)
			for (int i = 0; i < NumOfSIMD(MAX_HITBOXES); i++)
				flags |= colliders[i].Intersect(lpData.eyePos, endPoint);

		if (!flags)
			target.id = -1;
		else {
			lpData.keys |= Keys::ATTACK1;
			cmd->buttons |= IN_ATTACK;
		}
	}

	aimbotTargets.Push(target);
}

static void ThreadedUpdate(UpdateData* data)
{
	UpdateBoundsStart(data->players, data->prevPlayers);
	UpdateBoundsEnd(data->players, data->prevPlayers);
	UpdateVelocity(data->players, data->prevPlayers);
	UpdateHealth(data->players, data->prevPlayers);
	UpdateArmor(data->players, data->prevPlayers);
}

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
	if (FwBridge::localPlayer && ent->teamNum() == FwBridge::localPlayer->teamNum())
		cflags |= Flags::FRIENDLY;
}

static void UpdateOrigin(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.origin[i] = ent->origin();
		else if (players.flags[i] & Flags::EXISTS) {
			int pID = prevPlayers.sortIDs[players.unsortIDs[i]];
			if (pID < prevPlayers.count)
				players.origin[i] = prevPlayers.origin[pID];
		}
	}
}

static void UpdateBoundsStart(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.boundsStart[i / SIMD_COUNT].acc[i % SIMD_COUNT] = ent->mins();
		else if (players.flags[i] & Flags::EXISTS) {
			int pID = prevPlayers.sortIDs[players.unsortIDs[i]];
			if (pID < prevPlayers.count)
				players.boundsStart[i / SIMD_COUNT].acc[i % SIMD_COUNT].Set(prevPlayers.boundsStart[pID / SIMD_COUNT].acc[pID % SIMD_COUNT]);
		}
	}
}

static void UpdateBoundsEnd(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.boundsStart[i / SIMD_COUNT].acc[i % SIMD_COUNT] = ent->maxs();
		else if (players.flags[i] & Flags::EXISTS) {
			int pID = prevPlayers.sortIDs[players.unsortIDs[i]];
			if (pID < prevPlayers.count)
				players.boundsEnd[i / SIMD_COUNT].acc[i % SIMD_COUNT].Set(prevPlayers.boundsEnd[pID / SIMD_COUNT].acc[pID % SIMD_COUNT]);
		}
	}
}

alignas(SIMD_COUNT * 4)
static matrix3x4_t* boneMatrix = nullptr;
static mstudiobbox_t* hitboxes[MAX_HITBOXES];
static mstudiobone_t* bones[MAX_HITBOXES];
static int boneIDs[MAX_HITBOXES];
static float radius[MAX_HITBOXES];
static float damageMul[MAX_HITBOXES];

/*
  This function is a bit too long.
  We could split it to smaller chunks.
*/

static void UpdateHitboxes(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		HitboxList& hbList = players.hitboxes[i];
		int hb = -1;
		if (players.flags[i] & Flags::UPDATED) {
			studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());
			if (!hdr)
				continue;

			mstudiohitboxset_t* set = hdr->GetHitboxSet(0);
			if (!set)
				continue;

			boneMatrix = players.bones[i];

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
				bones[hb] = hdr->GetBone(hitboxes[hb]->bone);
				boneIDs[hb] = hitboxes[hb]->bone;
				radius[hb] = hitboxes[hb]->radius;
				HitGroups hitGroup = (HitGroups)hitboxes[hb]->group;

				bool hasHeavyArmor = ent->hasHeavyArmor();

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
					hbList.start[idx] = hitboxes[idx]->bbmin;

			for (int idx = 0; idx < MAX_HITBOXES; idx++)
				if (hitboxes[idx])
					hbList.end[idx] = hitboxes[idx]->bbmax;

			for (int idx = 0; idx < MAX_HITBOXES; idx++)
				hbList.wm[idx] = boneMatrix[boneIDs[idx]];

			for (int idx = 0; idx < MAX_HITBOXES; idx++)
				hbList.radius[idx] = radius[idx];

			for (int idx = 0; idx < MAX_HITBOXES; idx++)
				hbList.damageMul[idx] = damageMul[idx];

			vec3_t camDir[MAX_HITBOXES];
			vec3_t hUp[MAX_HITBOXES];

			for (int idx = 0; idx < MAX_HITBOXES; idx++)
				camDir[idx] = FwBridge::lpData.eyePos;

			for (int idx = 0; idx < MAX_HITBOXES; idx++) {
				hUp[idx] = (vec3_t)hbList.wm[idx].vec.acc[3];
				hUp[idx].z += 1;
			}

			for (int idx = 0; idx < MAX_HITBOXES; idx++) {
				matrix3x4_t transpose = hbList.wm[idx].InverseTranspose();
				camDir[idx] = transpose.Vector3ITransform(camDir[idx]);
				hUp[idx] = transpose.Vector3ITransform(hUp[idx]);
			}

			for (int idx = 0; idx < MAX_HITBOXES; idx++)
				hUp[idx].Normalize();

			for (int idx = 0; idx < MAX_HITBOXES; idx++) {
				/*
					MULTIPOINT_COUNT is 8
					We will have 2 points on each end of the hitbox axis
					and 3 points on each end of the hitbox
					(extending along the axis, and perpendicular to the axis).
					Side points will be rotated to face towards the camera,
					thus are recalculated each tick.
					Some hitboxes are box shaped and are needed to be set up differently.
				*/

				vecSoa<float, MULTIPOINT_COUNT, 3> tOffset;
				vecSoa<float, MULTIPOINT_COUNT, 3> tDir;

				int o = 0;

				if (hitboxes[idx]->radius >= 0) {
					vec3_t dir = hitboxes[idx]->bbmax - hitboxes[idx]->bbmin;
					dir.Normalize();

					vec3_t camCross = camDir[idx].Cross(dir);
					camCross.Normalize();

					vec3_t camCrossUp = camDir[idx].Cross(hUp[idx]).Cross(camDir[idx]);
					camCrossUp.Normalize();

					vec3_t camDirUp = camCross.Cross(camDir[idx]);
					camDirUp.Normalize();

					tOffset.AssignCol(o, hitboxes[idx]->bbmin);
					tDir.AssignCol(o++, 0);
					tOffset.AssignCol(o, hitboxes[idx]->bbmax);
					tDir.AssignCol(o++, 0);

					tOffset.AssignCol(o, hitboxes[idx]->bbmin);
					tDir.AssignCol(o++, camCrossUp * -1.f);
					tOffset.AssignCol(o, hitboxes[idx]->bbmax);
					tDir.AssignCol(o++, camCrossUp);

					tOffset.AssignCol(o, hitboxes[idx]->bbmin);
					tDir.AssignCol(o++, camCross * -1.f);
					tOffset.AssignCol(o, hitboxes[idx]->bbmin);
					tDir.AssignCol(o++, camCross);

					tOffset.AssignCol(o, hitboxes[idx]->bbmax);
					tDir.AssignCol(o++, camCross * -1.f);
					tOffset.AssignCol(o, hitboxes[idx]->bbmax);
					tDir.AssignCol(o++, camCross);
				} else {

					//TODO: fix box orientation.

					vec3_t bbmax = hitboxes[idx]->bbmax;
					vec3_t bbmin = hitboxes[idx]->bbmin;

					vec3_t s = bbmin;
					tOffset.AssignCol(o, s);
					tDir.AssignCol(o++, 0);

					s = hitboxes[idx]->bbmax;
					tOffset.AssignCol(o, s);
					tDir.AssignCol(o++, 0);

					for (int q = 0; q < 3; q++) {
						s = bbmin;
						s[q] = bbmax[q];
						tOffset.AssignCol(o, s);
						tDir.AssignCol(o++, 0);

						s = bbmax;
						s[q] = bbmin[q];
						tOffset.AssignCol(o, s);
						tDir.AssignCol(o++, 0);
					}
				}

				hbList.mpOffset[idx] = tOffset.Rotate();
				auto rot = tDir.Rotate();
				hbList.mpDir[idx] = rot;

			}

			players.flags[i] |= Flags::HITBOXES_UPDATED;
		}
		else if (players.flags[i] & Flags::EXISTS) {
			int pID = prevPlayers.sortIDs[players.unsortIDs[i]];
			if (pID < prevPlayers.count)
				players.hitboxes[i] = prevPlayers.hitboxes[pID];
		}
	}
}

static void UpdateColliders(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		if (players.flags[i] & Flags::HITBOXES_UPDATED) {
			for (int o = 0; o < NumOfSIMD(MAX_HITBOXES); o++) {

				HitboxList& hitboxes = players.hitboxes[i];

				vecSoa<float, SIMD_COUNT, 3> start, end;

				for (int u = 0; u < SIMD_COUNT; u++)
					start.AssignCol(u, hitboxes.wm[o * SIMD_COUNT + u].Vector3Transform(hitboxes.start[o * SIMD_COUNT + u]));

				players.colliders[i][o].start = start.Rotate();

				for (int u = 0; u < SIMD_COUNT; u++)
					end.AssignCol(u, hitboxes.wm[o * SIMD_COUNT + u].Vector3Transform(hitboxes.end[o * SIMD_COUNT + u]));

				players.colliders[i][o].end = end.Rotate();

				for (int u = 0; u < SIMD_COUNT; u++)
					players.colliders[i][o].radius[u] = hitboxes.radius[o * SIMD_COUNT + u];
			}
		} else {
			int pID = prevPlayers.sortIDs[players.unsortIDs[i]];
			if (pID < prevPlayers.count)
				for (int o = 0; o < NumOfSIMD(MAX_HITBOXES); o++)
					players.colliders[i][o] = prevPlayers.colliders[pID][o];
		}
	}
}

static void UpdateVelocity(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.velocity[i] = ent->velocity();
		else if (players.flags[i] & Flags::EXISTS) {
			int pID = prevPlayers.sortIDs[players.unsortIDs[i]];
			if (pID < prevPlayers.count)
				players.velocity[i] = prevPlayers.velocity[pID];
		}
	}
}

static void UpdateHealth(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.health[i] = ent->health();
		else if (players.flags[i] & Flags::EXISTS) {
			int pID = prevPlayers.sortIDs[players.unsortIDs[i]];
			if (pID < prevPlayers.count)
				players.health[i] = prevPlayers.health[pID];
		}
	}
}

static void UpdateArmor(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		if (players.flags[i] & Flags::UPDATED)
			players.armor[i] = ent->armorValue();
		else if (players.flags[i] & Flags::EXISTS) {
			int pID = prevPlayers.sortIDs[players.unsortIDs[i]];
			if (pID < prevPlayers.count)
				players.armor[i] = prevPlayers.armor[pID];
		}
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
