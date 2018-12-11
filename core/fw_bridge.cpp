#include "fw_bridge.h"

#define SOURCE_DEFINITIONS
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
#include "lagcompensation.h"
#include "tracing.h"
#include "settings.h"

#include <algorithm>


C_BasePlayer* FwBridge::localPlayer = nullptr;
int FwBridge::playerCount = 0;
uint64_t FwBridge::playersFl = 0;
C_BaseCombatWeapon* FwBridge::activeWeapon = nullptr;
float FwBridge::backtrackCurtime = 0;
int FwBridge::hitboxIDs[Hitboxes::HITBOX_MAX];
HistoryList<Players, BACKTRACK_TICKS> FwBridge::playerTrack;
LocalPlayer FwBridge::lpData;
HistoryList<AimbotTarget, BACKTRACK_TICKS> FwBridge::aimbotTargets;
HistoryList<unsigned int, BACKTRACK_TICKS> FwBridge::aimbotTargetIntersects;
uint64_t FwBridge::immuneFlags = 0;


static ConVar* weapon_recoil_scale = nullptr;

struct SortData {
	C_BasePlayer* player;
	float fov;
	int id;
};

static SortData players[64];


static void ExecuteAimbot(CUserCmd* cmd, bool* bSendPacket, FakelagState_t state);
static void ThreadedUpdate(UpdateData* data);
static bool PlayerSort(SortData& a, SortData& b);
static void UpdateFlags(int& flags, int& cflags, C_BasePlayer* ent);

/*
  We try to be as much cache efficient as possible here.
  Thus, we split each data entry to it's own separate function,
  since that way memory read/write will be sequencial on our side.
  About the game side - not much you can do.
*/
static void SwitchFlags(Players& __restrict players, Players& __restrict prevPlayers);
static void UpdateHitboxes(Players& __restrict players, const std::vector<int>* updatedList, std::vector<int>* updatedHitboxList);
static void UpdateColliders(Players& __restrict players, const std::vector<int>* updatedHitboxList);

static void MoveOrigin(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList);
static void MoveEyePos(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList);
static void MoveBoundsStart(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList);
static void MoveBoundsEnd(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList);
static void MoveVelocity(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList);
static void MoveHealth(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList);
static void MoveArmor(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList);
static void MoveHitboxes(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList);
static void MoveColliders(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList);

static ConVar* game_type = nullptr;
static ConVar* game_mode = nullptr;

bool FwBridge::IsEnemy(C_BasePlayer* ent)
{
	if (!game_type)
		game_type = cvar->FindVar(ST("game_type"));

	if (!game_mode)
		game_mode = cvar->FindVar(ST("game_mode"));

	if (game_type->GetInt() == 6 && !game_mode->GetInt()) {
		if (FwBridge::localPlayer->survivalTeamNum() == -1)
			return true;
		return FwBridge::localPlayer->survivalTeamNum() ^ ent->survivalTeamNum();
	}

	return ent->teamNum() ^ FwBridge::localPlayer->teamNum();
}

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

	//TODO: Clean this up
#ifdef _WIN32
	Weapon_ShootPosition(localPlayer, lpData.eyePos);
#else
	lpData.eyePos = Weapon_ShootPosition(localPlayer);
#endif

	lpData.velocity = localPlayer->velocity();
	lpData.origin = localPlayer->origin();
	lpData.time = globalVars->interval_per_tick * localPlayer->tickBase();

	float recoilScale = 1.f;

	if (!weapon_recoil_scale)
		weapon_recoil_scale = cvar->FindVar(ST("weapon_recoil_scale"));

	if (weapon_recoil_scale)
		recoilScale = weapon_recoil_scale->GetFloat();

	lpData.aimOffset = localPlayer->aimPunchAngle() * recoilScale;

	int flags = localPlayer->flags();
	int cflags = 0;
	UpdateFlags(flags, cflags, localPlayer);
	lpData.flags = cflags;

	SourceEssentials::UpdateData(cmd, &lpData);
}

static std::vector<int> updatedPlayers;
static std::vector<int> nonUpdatedPlayers;

void FwBridge::UpdatePlayers(CUserCmd* cmd)
{
	UpdateData data(playerTrack.Push(), playerTrack.GetLastItem(1), &updatedPlayers, &nonUpdatedPlayers, false);
	data.players.count = engine->GetMaxClients();
	data.players.globalTime = globalVars->curtime;

	updatedPlayers.clear();
	nonUpdatedPlayers.clear();

	int count = 0;

	for (int i = 0; i < MAX_PLAYERS; i++)
		data.players.sortIDs[i] = MAX_PLAYERS;

	playerCount = 0;
	playersFl = 0;
	immuneFlags = 0;

	for (int i = 1; i < 64; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)entityList->GetClientEntity(i);

		if (!ent)
			continue;

		bool player = ent->IsPlayer();
		bool dormant = ent->IsDormant();

		if (!ent || !player || dormant || i == 0)
			continue;

		playerCount = i;

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

		playersFl |= (1ull << i);

		vec3_t origin = ent->origin();
		origin.z += ((1.f - ent->duckAmount()) * 18.f + 46) / 2;
		vec3_t angle = ((vec3_t)origin - lpData.eyePos).GetAngles(true);
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

		data.players.time[i] = ent->simulationTime() - 1;

		int flags = ent->flags();
		int cflags;
		UpdateFlags(flags, cflags, ent);

	    if (ent->health() <= 0 || ent->gunGameImmunity())
			FwBridge::immuneFlags |= 1ull << players[i].id;

		int pID = data.players.Resort(data.prevPlayers, i);
		if (ent->lifeState() == LIFE_ALIVE && (pID >= MAX_PLAYERS || data.players.time[i] != data.prevPlayers.time[pID])) {
			cflags |= Flags::UPDATED;
			data.players.time[i] = ent->simulationTime();
			updatedPlayers.push_back(i);
		} else if (data.players.Resort(data.prevPlayers, i) < data.prevPlayers.count)
			nonUpdatedPlayers.push_back(i);

		data.players.flags[i] = cflags;
	}
	data.players.count = count;

	Engine::StartLagCompensation();

	//We want to push empty lists only if the previous list was not empty.
	if (count > 0 || data.prevPlayers.count > 0) {
		FinishUpdating(&data);
		LagCompensation::Run();
	} else
		playerTrack.UndoPush();
}

static std::vector<int> hitboxUpdatedList;

void FwBridge::FinishUpdating(UpdateData* data)
{
	//Updating the hitboxes calls engine functions that only work on the main thread
	//While it is being done, let's update other data on a seperate thread
	//Flags depend on the animation fix fixing up the player.
	MoveOrigin(data->players, data->prevPlayers, data->nonUpdatedPlayers);
	MoveEyePos(data->players, data->prevPlayers, data->nonUpdatedPlayers);

	for (int i : *data->updatedPlayers) {
		C_BasePlayer* ent = (C_BasePlayer*)data->players.instance[i];
		data->players.origin[i] = ent->origin();
		float eyeOffset = (1.f - ent->duckAmount()) * 18.f + 46;
		data->players.eyePos[i] = data->players.origin[i];
		data->players.eyePos[i][2] += eyeOffset;
	}


	if (!data->additionalUpdate) {
		Impacts::Tick();
		Engine::StartAnimationFix(&data->players, &data->prevPlayers);
	}

	hitboxUpdatedList.clear();

	Threading::QueueJobRef(ThreadedUpdate, data);
	UpdateHitboxes(data->players, data->updatedPlayers, &hitboxUpdatedList);
	UpdateColliders(data->players, &hitboxUpdatedList);
	Threading::FinishQueue();
	SwitchFlags(data->players, data->prevPlayers);
}

static float prevBacktrackCurtime = 0;
static int prevtc = 0;

void FwBridge::RunFeatures(CUserCmd* cmd, bool* bSendPacket, void* hostRunFrameFp)
{
	backtrackCurtime = Settings::aimbotBacktrack ? Engine::CalculateBacktrackTime() + globalVars->interval_per_tick : TicksToTime(cmd->tick_count) + Engine::LerpTime();

	if (Settings::aimbotSafeBacktrack)
		backtrackCurtime = std::max(backtrackCurtime, prevBacktrackCurtime);

	prevBacktrackCurtime = backtrackCurtime;

	if (Settings::bunnyhopping)
		SourceBhop::Run(cmd, &lpData);

	if (Settings::autostrafer)
		SourceAutostrafer::Run(cmd, &lpData);

	FakelagState_t state = Settings::fakelag ? SourceFakelag::Run(cmd, &lpData, bSendPacket, !*((long*)hostRunFrameFp - RUNFRAME_TICK)) : FakelagState::LAST | FakelagState::FIRST;

	if (Settings::aimbot)
		ExecuteAimbot(cmd, bSendPacket, state);

	if (Settings::antiaim)
		Antiaim::Run(cmd, state);

	SourceEssentials::UpdateCMD(cmd, &lpData);
	SourceEnginePred::Finish(cmd, localPlayer);
	Engine::EndLagCompensation();

	prevtc = cmd->tick_count;

	Visuals::shouldDraw = true;
}


//This way it is way cleaner
using namespace FwBridge;

static bool allowShoot = false;
float lastPrimary = 0.f;

void RenderPlayerCapsules(Players& pl, Color col, int id = -1);
extern int btTick;

//#ifdef DEBUG
static HistoryList<int, 64> traceCountHistory;
static HistoryList<unsigned long, 64> traceTimeHistory;
int FwBridge::traceCountAvg = 0;
int FwBridge::traceTimeAvg = 0;

#include <chrono>
typedef std::chrono::high_resolution_clock Clock;
//#endif

static void ExecuteAimbot(CUserCmd* cmd, bool* bSendPacket, FakelagState_t state)
{
	//Aimbot part
	AimbotTarget target;
	target.id = -1;

	auto& targetIntersects = aimbotTargetIntersects.Push();
	targetIntersects = 0;

	if (activeWeapon) {
		//We can only shoot once until we take a shot
		if (lastPrimary != activeWeapon->nextPrimaryAttack())
			allowShoot = false;
		lastPrimary = activeWeapon->nextPrimaryAttack();
		if (!allowShoot)
			allowShoot = state & FakelagState::LAST;
		if (!allowShoot)
			lpData.keys &= ~Keys::ATTACK1;

		btTick = -1;

		auto* track = &FwBridge::playerTrack;

		if (allowShoot && activeWeapon->nextPrimaryAttack() <= globalVars->curtime && (Settings::aimbotAutoShoot || lpData.keys & Keys::ATTACK1)) {
			unsigned char hitboxList[MAX_HITBOXES];
		    float pointScale[MAX_HITBOXES];
			memset(hitboxList, 0, sizeof(hitboxList));

			for (auto& i : Settings::aimbotHitboxes) {
				printf("PHB: %d %d %d\n", i.hitbox, hitboxIDs[i.hitbox], i.mask);
				if (i.hitbox >= 0 && hitboxIDs[i.hitbox] >= 0) {
					hitboxList[hitboxIDs[i.hitbox]] = i.mask;
					pointScale[hitboxIDs[i.hitbox]] = i.pointScale;
				}
			}

//#ifdef DEBUG
			Tracing2::ResetTraceCount();

			auto t1 = Clock::now();
//#endif
			target = Aimbot::RunAimbot(track, Settings::aimbotLagCompensation ? LagCompensation::futureTrack : nullptr, &lpData, hitboxList, &immuneFlags, pointScale);
//#ifdef DEBUG
			auto t2 = Clock::now();

			traceCountHistory.Push(Tracing2::RetreiveTraceCount());
			traceTimeHistory.Push(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());

			traceTimeAvg = 0;
			traceCountAvg = 0;

			int u = 0;

			for (; u < traceCountHistory.Count(); u++) {
				traceCountAvg += traceCountHistory[u];
				traceTimeAvg += traceTimeHistory[u];
			}

			if (u) {
				traceCountAvg /= u;
				traceTimeAvg /= u;
			}
//#endif

			if (target.future)
				track = LagCompensation::futureTrack;

			if (target.id >= 0) {
				btTick = target.backTick;
				cmd->tick_count = TimeToTicks(track->GetLastItem(target.backTick).time[target.id] + Engine::LerpTime());
				if (!Spread::HitChance(&track->GetLastItem(target.backTick), target.id, target.targetVec, target.boneID, Settings::aimbotHitChance))
					lpData.keys &= ~Keys::ATTACK1;

#ifdef PT_VISUALS
				if (false && btTick >= 0)
					RenderPlayerCapsules(track->GetLastItem(btTick), Color(0.f, 1.f, 0.f, 1.f), target.id);
#endif
			} else
				lpData.angles -= lpData.aimOffset;
		}

		if (btTick > BACKTRACK_TICKS)
			btTick = BACKTRACK_TICKS;

		if (target.id >= 0) {
			vec3_t dir, up, down;
			(lpData.angles + lpData.aimOffset).GetVectors(dir, up, down, true);
			vec3_t endPoint = dir * lpData.weaponRange + lpData.eyePos;

			CapsuleColliderSOA<SIMD_COUNT>* colliders = track->GetLastItem(target.backTick).colliders[target.id];

			unsigned int flags = 0;

			if (1 || track->GetLastItem(target.backTick).flags[target.id] & Flags::ONGROUND)//lpData.keys & Keys::ATTACK1)
				for (int i = 0; i < NumOfSIMD(MAX_HITBOXES); i++)
					flags |= colliders[i].Intersect(lpData.eyePos, endPoint) << (16u * i);

			if (false && !flags)
				target.id = -1;

			else if (Settings::aimbotAutoShoot)
				lpData.keys |= Keys::ATTACK1;

			targetIntersects = flags;
		}

		if (lpData.keys & Keys::ATTACK1)
			Spread::CompensateSpread(cmd);
	}

	//Disable the actual aimbot for now
	if (!Settings::aimbotSetAngles)
		lpData.angles = cmd->viewangles;

	aimbotTargets.Push(target);
}

static void ThreadedUpdate(UpdateData* data)
{
	for (int i : *data->updatedPlayers) {
		C_BasePlayer* ent = (C_BasePlayer*)data->players.instance[i];
		data->players.boundsStart[i] = ent->mins();
		data->players.boundsEnd[i] = ent->maxs();
		data->players.velocity[i] = ent->velocity();
		data->players.health[i] = ent->health();
		data->players.armor[i] = ent->armorValue();
	}

	MoveBoundsStart(data->players, data->prevPlayers, data->nonUpdatedPlayers);
	MoveBoundsEnd(data->players, data->prevPlayers, data->nonUpdatedPlayers);
	MoveVelocity(data->players, data->prevPlayers, data->nonUpdatedPlayers);
	MoveHealth(data->players, data->prevPlayers, data->nonUpdatedPlayers);
	MoveArmor(data->players, data->prevPlayers, data->nonUpdatedPlayers);

	MoveHitboxes(data->players, data->prevPlayers, data->nonUpdatedPlayers);
	MoveColliders(data->players, data->prevPlayers, data->nonUpdatedPlayers);

	if (!data->additionalUpdate)
		LagCompensation::PreRun();
}

//Sort the players for better data layout, in this case - by FOV
static bool PlayerSort(SortData& a, SortData& b)
{
    return a.fov < b.fov;
}

static void MoveBoundsStart(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList)
{
	for (int i : *nonUpdatedList) {
		int pID = players.Resort(prevPlayers, i);
		players.boundsStart[i] = prevPlayers.boundsStart[pID];
	}
}

static void MoveBoundsEnd(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList)
{
	for (int i : *nonUpdatedList) {
		int pID = players.Resort(prevPlayers, i);
		players.boundsEnd[i] = prevPlayers.boundsEnd[pID];
	}
}

static void MoveVelocity(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList)
{
	for (int i : *nonUpdatedList)
		players.velocity[i] = prevPlayers.velocity[players.Resort(prevPlayers, i)];
}

static void MoveHealth(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList)
{
	for (int i : *nonUpdatedList)
		players.health[i] = prevPlayers.health[players.Resort(prevPlayers, i)];
}

static void MoveArmor(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList)
{
	for (int i : *nonUpdatedList)
		players.armor[i] = prevPlayers.armor[players.Resort(prevPlayers, i)];
}

static void MoveOrigin(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList)
{
	for (int i : *nonUpdatedList)
		players.origin[i] = prevPlayers.origin[players.Resort(prevPlayers, i)];
}

static void MoveEyePos(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList)
{
	for (int i : *nonUpdatedList)
		players.eyePos[i] = prevPlayers.eyePos[players.Resort(prevPlayers, i)];
}

static void MoveColliders(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList)
{
	for (int i : *nonUpdatedList) {
		int pID = players.Resort(prevPlayers, i);
		for (int o = 0; o < NumOfSIMD(MAX_HITBOXES); o++)
			players.colliders[i][o] = prevPlayers.colliders[pID][o];
	}
}

static void MoveHitboxes(Players& __restrict players, Players& __restrict prevPlayers, const std::vector<int>* nonUpdatedList)
{
	for (int i : *nonUpdatedList) {
		int pID = players.Resort(prevPlayers, i);
		players.hitboxes[i] = prevPlayers.hitboxes[pID];
		players.flags[i] |= prevPlayers.flags[pID] & Flags::HITBOXES_UPDATED;
	}
}

static void UpdateFlags(int& flags, int& cflags, C_BasePlayer* ent)
{
	cflags = Flags::EXISTS;
	if (flags & FL_ONGROUND)
		cflags |= Flags::ONGROUND;
	if (flags & FL_DUCKING)
		cflags |= Flags::DUCKING;
	if (FwBridge::localPlayer) {
		if (!FwBridge::IsEnemy(ent))
			cflags |= Flags::FRIENDLY;
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

static void UpdateCapsuleHitbox(int idx, HitboxList* hbList, vec3_t camDir[MAX_HITBOXES], vec3_t hUp[MAX_HITBOXES])
{
	vecSoa<float, MULTIPOINT_COUNT, 3> tOffset;
	vecSoa<float, MULTIPOINT_COUNT, 3> tDir;

	int o = 0;

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

	hbList->mpOffset[idx] = tOffset.Rotate();
	auto rot = tDir.Rotate();
	hbList->mpDir[idx] = rot;
}

static void UpdateHitbox(int idx, HitboxList* hbList)
{
	vecSoa<float, MULTIPOINT_COUNT, 3> tOffset;
	vecSoa<float, MULTIPOINT_COUNT, 3> tDir;

	int o = 0;

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

	hbList->mpOffset[idx] = tOffset.Rotate();
	auto rot = tDir.Rotate();
	hbList->mpDir[idx] = rot;
}

static void UpdateHitboxes(Players& __restrict players, const std::vector<int>* updatedPlayers, std::vector<int>* updatedHitboxPlayers)
{
	for (int i : *updatedPlayers) {

		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];

		studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());
		if (!hdr)
			continue;

		mstudiohitboxset_t* set = hdr->GetHitboxSet(0);
		if (!set)
			continue;

		if (!Engine::UpdatePlayer(ent, players.bones[i]))
			continue;

		updatedHitboxPlayers->push_back(i);
	}

	for (int i : *updatedHitboxPlayers) {
		boneMatrix = players.bones[i];

		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());
		mstudiohitboxset_t* set = hdr->GetHitboxSet(0);

		HitboxList& hbList = players.hitboxes[i];
		int hb = -1;

		for (int idx = 0; idx < set->numhitboxes; idx++)
			FwBridge::hitboxIDs[idx] = -1;

		for (int idx = 0; idx < set->numhitboxes && hb < MAX_HITBOXES - 1; idx++) {
			if (idx == Hitboxes::HITBOX_UPPER_CHEST ||
				idx == Hitboxes::HITBOX_LEFT_UPPER_ARM || idx == Hitboxes::HITBOX_RIGHT_UPPER_ARM) {
				continue;
			}

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

		for (int idx = 0; idx < MAX_HITBOXES; idx++)
			if (hitboxes[idx])
				hbList.start[idx] = hitboxes[idx]->bbmin;

		for (int idx = 0; idx < MAX_HITBOXES; idx++)
			if (hitboxes[idx])
				hbList.end[idx] = hitboxes[idx]->bbmax;

		for (int idx = 0; idx < MAX_HITBOXES; idx++)
			hbList.wm[idx] = boneMatrix[boneIDs[idx]];

		//Fix box shaped hitbox orientataion. TODO: Come up with a better/less hard-coded way of solving this.
		matrix3x4_t rotMatrixFeet = matrix3x4_t::GetMatrix(vec3_t(0, 25, 0), true);
		vec3 offsetVecFeet = rotMatrixFeet.Vector3Rotate(vec3_t(0, 1, 0)) * 4.5f;
		matrix3x4_t rotMatrixHand = matrix3x4_t::GetMatrix(vec3_t(0, 15, 0), true);

		matrix3x4_t& lfMatrix = hbList.wm[FwBridge::hitboxIDs[Hitboxes::HITBOX_LEFT_FOOT]];
		lfMatrix.vec.AddRow(3, lfMatrix.Vector3Transform(offsetVecFeet * -1.f) - (vec3)lfMatrix.vec.acc[3]);
		lfMatrix *= rotMatrixFeet;

		matrix3x4_t& rfMatrix = hbList.wm[FwBridge::hitboxIDs[Hitboxes::HITBOX_RIGHT_FOOT]];
		rfMatrix.vec.AddRow(3, rfMatrix.Vector3Transform(offsetVecFeet) - (vec3)rfMatrix.vec.acc[3]);
		rfMatrix *= rotMatrixFeet;

		hbList.wm[FwBridge::hitboxIDs[Hitboxes::HITBOX_LEFT_HAND]] *= rotMatrixHand;
		hbList.wm[FwBridge::hitboxIDs[Hitboxes::HITBOX_RIGHT_HAND]] *= rotMatrixHand;

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

			if (hitboxes[idx]->radius >= 0)
				UpdateCapsuleHitbox(idx, &hbList, camDir, hUp);
			else
				UpdateHitbox(idx, &hbList);
		}

		players.flags[i] |= Flags::HITBOXES_UPDATED;
	}
}

static void UpdateColliders(Players& __restrict players, const std::vector<int>* updatedHitboxPlayers)
{
	for (int i : *updatedHitboxPlayers) {
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
	}
}

static void SwitchFlags(Players& __restrict players, Players& __restrict prevPlayers)
{
	for (int i = 0; i < players.count; i++) {
		int pID = players.Resort(prevPlayers, i);
		if (pID < prevPlayers.count && players.flags[i] & Flags::EXISTS && (~players.flags[i]) & Flags::UPDATED && prevPlayers.flags[pID] & Flags::UPDATED && players.instance[i] == prevPlayers.instance[pID]) {
			int fl = players.flags[i];
			players.flags[i] = prevPlayers.flags[pID];
			prevPlayers.flags[pID] = fl;
		}
	}
}
