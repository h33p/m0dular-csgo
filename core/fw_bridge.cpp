#include "fw_bridge.h"

#define SOURCE_DEFINITIONS
#include "../sdk/framework/utils/threading.h"
#include "../sdk/framework/utils/stackstring.h"
#include "../sdk/features/features.h"
#include "../sdk/framework/utils/intersect_impl.h"
#include "../sdk/framework/features/aimbot.h"

#include "engine.h"
#include "hooks.h"
#include "tracing.h"
#include "settings.h"
#include "mtr_scoped.h"

#include "../features/spread.h"
#include "../features/visuals.h"
#include "../features/impacts.h"
#include "../features/antiaim.h"
#include "../features/lagcompensation.h"
#include "../features/awall.h"

#include <algorithm>
#include <chrono>

typedef std::chrono::high_resolution_clock Clock;

C_BasePlayer* FwBridge::localPlayer = nullptr;
C_BasePlayer* FwBridge::playerList[MAX_PLAYERS];
int FwBridge::playerCount = 0;
uint64_t FwBridge::playersFl = 0;
C_BaseCombatWeapon* FwBridge::activeWeapon = nullptr;
CCSWeaponInfo* FwBridge::weaponInfo = nullptr;
float FwBridge::backtrackCurtime = 0;
int FwBridge::hitboxIDs[Hitboxes::HITBOX_MAX];
int FwBridge::reHitboxIDs[MAX_HITBOXES];
studiohdr_t* FwBridge::cachedHDRs[MAX_PLAYERS];
HistoryList<Players, BACKTRACK_TICKS> FwBridge::playerTrack;
bool FwBridge::curPushed = false;
LocalPlayer FwBridge::lpData;
HistoryList<AimbotTarget, BACKTRACK_TICKS> FwBridge::aimbotTargets;
HistoryList<unsigned int, BACKTRACK_TICKS> FwBridge::aimbotTargetIntersects;
uint64_t FwBridge::immuneFlags = 0;
float FwBridge::originalLBY[MAX_PLAYERS];
std::vector<vec3_t> FwBridge::localPlayerAngles;
bool FwBridge::localPlayerSentPacket = false;
bool FwBridge::enableBoneSetup = true;

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
static void UpdateHitboxesPart1(Players& __restrict players, const std::vector<int>* updatedPlayers, bool async = false);
static void UpdateHitboxesPart2(Players& __restrict players, const std::vector<int>* updatedPlayers, std::vector<int>* updatedHitboxPlayers);
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

void FwBridge::HandleLBYProxy(C_BasePlayer* ent, float ang)
{
	if (!ent)
		return;

	int eID = ent->EntIndex();

	if (eID >= MAX_PLAYERS || eID < 0)
		return;

	originalLBY[eID] = ang;
	ent->lowerBodyYawTarget() = ang;
}

C_BasePlayer* FwBridge::GetPlayer(const Players& players, int entID)
{
	if (entID < 0 || entID >= MAX_PLAYERS)
		return nullptr;

	int uid = players.unsortIDs[entID];

	if (uid < 0 || uid >= MAX_PLAYERS)
		return nullptr;

	return playerList[uid];
}

void FwBridge::UpdateLocalPlayer()
{
	localPlayer = (C_BasePlayer*)entityList->GetClientEntity(engine->GetLocalPlayer());
	activeWeapon = nullptr;
	weaponInfo = nullptr;

	if (!localPlayer)
		return;

	activeWeapon = localPlayer->activeWeapon();

	if (activeWeapon)
		weaponInfo = GetWeaponInfo(weaponDatabase, activeWeapon->itemDefinitionIndex());
}

void FwBridge::UpdateLocalData(CUserCmd* cmd, void* hostRunFrameFp)
{
	MTR_BEGIN("FwBridge", "UpdateLocalData");

	UpdateLocalPlayer();

	//TODO: Move the settings part to a separate function
#ifdef TESTING_FEATURES
	SourceFakelag::ticksToChoke = Settings::fakelag;
	SourceFakelag::breakLagCompensation = Settings::fakelagBreakLC;
#endif

	if (activeWeapon) {
		lpData.weaponAmmo = activeWeapon->clip1();
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

	MTR_BEGIN("SourceEnginePred", "EnginePrediction");
	SourceEnginePred::Prepare(cmd, localPlayer, hostRunFrameFp);
	SourceEnginePred::Run(cmd, localPlayer);
	MTR_END("SourceEnginePred", "EnginePrediction");

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

	lpData.aimOffset = Engine::PredictAimPunchAngle() * recoilScale;

	int flags = localPlayer->flags();
	int cflags = 0;
	UpdateFlags(flags, cflags, localPlayer);
	lpData.flags = cflags;

	SourceEssentials::UpdateData(cmd, &lpData);
	MTR_END("FwBridge", "UpdateLocalData");
}

static std::vector<int> updatedPlayers;
static std::vector<int> nonUpdatedPlayers;
static std::map<C_BasePlayer*, bool> entitiesToUnhook;

void FwBridge::UpdatePlayers(CUserCmd* cmd)
{
	MTR_BEGIN("FwBridge", "UpdatePlayers");
	UpdateData data(playerTrack.Push(), playerTrack.GetLastItem(1), &updatedPlayers, &nonUpdatedPlayers, false);
	data.players.count = engine->GetMaxClients();
	data.players.globalTime = globalVars->curtime;

	updatedPlayers.clear();
	nonUpdatedPlayers.clear();
	memset(playerList, 0, sizeof(playerList));

	int count = 0;

	for (int i = 0; i < MAX_PLAYERS; i++)
		data.players.sortIDs[i] = MAX_PLAYERS;

	playerCount = 0;
	playersFl = 0;
	immuneFlags = 0;

	MTR_BEGIN("FwBridge", "EntHooksCheck");

	entitiesToUnhook.clear();

	for (const auto& i : CSGOHooks::entityHooks)
		entitiesToUnhook[i.first] = true;

	MTR_END("FwBridge", "EntHooksCheck");

	MTR_BEGIN("FwBridge", "PreSortLoop");
	for (int i = 1; i < 64; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)entityList->GetClientEntity(i);

		if (!ent)
			continue;

		bool player = ent->IsPlayer();
		bool dormant = ent->IsDormant();

		if (!ent || !player || dormant || i == 0)
			continue;

		//Hook the entity
		C_BasePlayer* hookEnt = (C_BasePlayer*)((uintptr_t*)ent + 1);

		if (CSGOHooks::entityHooks.find(hookEnt) == CSGOHooks::entityHooks.end()) {
#ifdef DEBUG
			cvar->ConsoleDPrintf("Hooking %p\n", hookEnt);
#endif
			VFuncHook* hook = new VFuncHook((void*)hookEnt);
			CSGOHooks::entityHooks[hookEnt] = hook;
			//TODO: Figure out a way to hook the destructor
			hook->Hook(13, CSGOHooks::SetupBones);
		} else
			entitiesToUnhook[hookEnt] = false;

		playerCount = i;

		playerList[i] = ent;

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
	MTR_END("FwBridge", "PreSortLoop");

	for (auto& i : entitiesToUnhook)
	    if (i.second) {
#ifdef DEBUG
			cvar->ConsoleDPrintf("Unhooking %p\n", i.first);
#endif
			delete CSGOHooks::entityHooks[i.first];
			CSGOHooks::entityHooks.erase(i.first);
		}

	std::sort(players, players + count, PlayerSort);

	data.players.Allocate(count);

	MTR_BEGIN("FwBridge", "PostSortLoop");
	for (int i = 0; i < count; i++) {

		C_BasePlayer* ent = players[i].player;

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
	MTR_END("FwBridge", "PostSortLoop");

	Engine::StartLagCompensation();

	curPushed = false;

	//We want to push empty lists only if the previous list was not empty.
	if (count > 0 || data.prevPlayers.count > 0) {
		curPushed = true;
		FinishUpdating(&data);
		LagCompensation::Run();
	} else
		playerTrack.UndoPush();

	MTR_END("FwBridge", "UpdatePlayers");
}

static std::vector<int> hitboxUpdatedList;

void FwBridge::FinishUpdating(UpdateData* data)
{
    MTR_SCOPED_TRACE("FwBridge", "FinishUpdating");
	MoveOrigin(data->players, data->prevPlayers, data->nonUpdatedPlayers);
	MoveEyePos(data->players, data->prevPlayers, data->nonUpdatedPlayers);

	for (int i : *data->updatedPlayers) {
		C_BasePlayer* ent = FwBridge::GetPlayerFast(data->players, i);
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
	UpdateHitboxesPart1(data->players, data->updatedPlayers);
	Threading::FinishQueue(true);
	UpdateHitboxesPart2(data->players, data->updatedPlayers, &hitboxUpdatedList);
	UpdateColliders(data->players, &hitboxUpdatedList);
	SwitchFlags(data->players, data->prevPlayers);
}

//This is used to update each hitboxes at multiple time stamps (used in LagCompensation) in an effective and lockless way
static std::vector<int> updatedPlayersTemp;
static std::vector<int> nonUpdatedPlayersTemp;
static std::vector<int> hitboxUpdatedListTemp;

void FwBridge::StartUpdatingMultiWorld(MultiUpdateData* data, size_t startIDX)
{
	MTR_SCOPED_TRACE("FwBridge", "StartUpdatingMultiWorld");

	for (size_t i = startIDX; i < data->worldList.size(); i++) {
		updatedPlayersTemp.clear();

		for (const auto& o : data->updatedIndices) {
			int u = data->worldList[i]->sortIDs[o.first];

			if (u < 0 || u >= MAX_PLAYERS)
				continue;

			if (o.second == i)
				updatedPlayersTemp.push_back(u);
		}

		UpdateHitboxesPart1(*data->worldList[i], &updatedPlayersTemp, true);
	}
}

void FwBridge::FinishUpdatingMultiWorld(MultiUpdateData* data, size_t startIDX)
{
	MTR_SCOPED_TRACE("FwBridge", "FinishUpdatingMultiWorld");

	Threading::FinishQueue(true);

	for (size_t i = 1; i < data->worldList.size(); i++) {
		updatedPlayersTemp.clear();
		nonUpdatedPlayersTemp.clear();

		for (const auto& o : data->updatedIndices) {
			int u = data->worldList[i]->sortIDs[o.first];

			if (u < 0 || u >= MAX_PLAYERS)
				continue;

			if (o.second == i)
				updatedPlayersTemp.push_back(u);
			else if (data->worldList[i]->Resort(*data->worldList[i - 1], u) < MAX_PLAYERS)
				nonUpdatedPlayersTemp.push_back(u);
		}

		UpdateData tempData(*data->worldList[i], *data->worldList[i - 1], &updatedPlayersTemp, &nonUpdatedPlayersTemp, true);

		MoveOrigin(*data->worldList[i], *data->worldList[i - 1], &nonUpdatedPlayersTemp);
		MoveEyePos(*data->worldList[i], *data->worldList[i - 1], &nonUpdatedPlayersTemp);

		for (int o : *tempData.updatedPlayers) {
			C_BasePlayer* ent = GetPlayerFast(tempData.players, o);
		    tempData.players.origin[o] = ent->origin();
			float eyeOffset = (1.f - ent->duckAmount()) * 18.f + 46;
		    tempData.players.eyePos[o] = tempData.players.origin[o];
		    tempData.players.eyePos[o][2] += eyeOffset;
		}

		//Do not thread this because it we do not have anything concurrent at the moment
		ThreadedUpdate(&tempData);

		hitboxUpdatedListTemp.clear();
		UpdateHitboxesPart2(*data->worldList[i], &updatedPlayersTemp, &hitboxUpdatedListTemp);
		UpdateColliders(*data->worldList[i], &hitboxUpdatedListTemp);
		SwitchFlags(*data->worldList[i], *data->worldList[i - 1]);
	}
}

static float prevBacktrackCurtime = 0;
static int prevtc = 0;

void FwBridge::RunFeatures(CUserCmd* cmd, bool* bSendPacket, void* hostRunFrameFp)
{
	MTR_SCOPED_TRACE("FwBridge", "RunFeatures");

	backtrackCurtime = Settings::aimbotBacktrack ? Engine::CalculateBacktrackTime() + globalVars->interval_per_tick : TicksToTime(cmd->tick_count) + Engine::LerpTime();

#ifdef TESTING_FEATURES
	if (Settings::aimbotSafeBacktrack)
		backtrackCurtime = std::max(backtrackCurtime, prevBacktrackCurtime);
#endif

	prevBacktrackCurtime = backtrackCurtime;

	if (Settings::bunnyhopping)
		SourceBhop::Run(cmd, &lpData);

	if (Settings::autostrafer)
		SourceAutostrafer::Run(cmd, &lpData, Settings::autostraferControl);

	FakelagState_t state =
#ifdef TESTING_FEATURES
		Settings::fakelag ? SourceFakelag::Run(cmd, &lpData, bSendPacket, !*((long*)hostRunFrameFp - RUNFRAME_TICK)) :
#endif
		FakelagState::LAST | FakelagState::FIRST;

	if (Settings::aimbot)
		ExecuteAimbot(cmd, bSendPacket, state);

#ifdef TESTING_FEATURES
	if (Settings::antiaim)
		Antiaim::Run(cmd, state);
#endif

	SourceEssentials::UpdateCMD(cmd, &lpData);
	SourceEnginePred::Finish(cmd, localPlayer);

	if (curPushed)
		Engine::EndLagCompensation();

	prevtc = cmd->tick_count;

	Visuals::shouldDraw = true;

	FwBridge::localPlayerAngles.push_back(cmd->viewangles);
	if (*bSendPacket)
		FwBridge::localPlayerSentPacket = true;
}


//This way it is way cleaner
using namespace FwBridge;

static bool allowShoot = false;
float lastPrimary = 0.f;

extern int btTick;

static HistoryList<int, 64> traceCountHistory;
static HistoryList<unsigned long, 64> traceTimeHistory;
int FwBridge::traceCountAvg = 0;
int FwBridge::traceTimeAvg = 0;

bool prevPressed = true;

static vec3_t prevAimOffset(0);

static void ExecuteAimbot(CUserCmd* cmd, bool* bSendPacket, FakelagState_t state)
{
	MTR_SCOPED_TRACE("FwBridge", "ExecuteAimbot");

	//Aimbot part
	AimbotTarget target;
	target.id = -1;

	auto& targetIntersects = aimbotTargetIntersects.Push();
	targetIntersects = 0;

	if (lpData.keys & Keys::ATTACK2){
		vec3_t dir(0), r, u;
		lpData.angles.GetVectors(dir, r, u, true);
		int cacheSize = 0;
		bool permaCache[AutoWall::MAX_INTERSECTS * 2 + 2];
		trace_t outTraces[AutoWall::MAX_INTERSECTS * 2 + 2];
		float outDamages[AutoWall::MAX_INTERSECTS * 2 + 2];
		AutoWall::FireBulletWorld(lpData.eyePos, dir, lpData.weaponRange, lpData.weaponRangeModifier, lpData.weaponDamage, lpData.weaponPenetration, &cacheSize, permaCache, outTraces, outDamages);
		float fdmg = AutoWall::FireBulletPlayers(lpData.eyePos, dir, lpData.weaponRange, lpData.weaponRangeModifier, lpData.weaponDamage, lpData.weaponPenetration, lpData.weaponArmorPenetration, &cacheSize, permaCache, outTraces, outDamages, &playerTrack[0]);

		//cvar->ConsoleDPrintf("FDMG: %f\n", fdmg);
		vec3_t posOut[AutoWall::MAX_INTERSECTS * 4 + 4];

		for (int i = 0; i < cacheSize; i++) {
			posOut[i * 2] = outTraces[i].startpos;
			posOut[i * 2 + 1] = outTraces[i].endpos;
		}

		Visuals::SetAwallBoxes(posOut, outDamages, cacheSize, fdmg, lpData.weaponDamage);
		prevPressed = true;
	} else {
		if (prevPressed)
			Visuals::RenderAwallBoxes();
		prevPressed = false;
	}

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
				if (i.hitbox >= 0 && hitboxIDs[i.hitbox] >= 0) {
					hitboxList[hitboxIDs[i.hitbox]] = i.mask;
					pointScale[hitboxIDs[i.hitbox]] = i.pointScale;
				}
			}

			extern int minDamage;
			minDamage = Settings::aimbotMinDamage;
//#ifdef DEBUG

			auto t1 = Clock::now();
//#endif
			target = Aimbot::RunAimbot(track,
#ifdef TESTING_FEATURES
									   Settings::aimbotLagCompensation ? LagCompensation::futureTrack :
#endif
									   nullptr, &lpData, hitboxList, &immuneFlags, pointScale);
//#ifdef DEBUG
			auto t2 = Clock::now();

			traceCountHistory.Push(Tracing2::RetreiveTraceCount());
			traceTimeHistory.Push(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());

			traceTimeAvg = 0;
			traceCountAvg = 0;

		    size_t u = 0;

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

			//cvar->ConsoleDPrintf(ST("T: %d (%d)\n"), target.id, target.dmg);

			if (target.id >= 0 && !Spread::HitChance(&track->GetLastItem(target.backTick), target.id, target.targetVec, target.boneID, Settings::aimbotHitChance)) {
				target.id = -1;
				lpData.keys &= ~Keys::ATTACK1;
			} else if (target.id >= 0)
			    lpData.angles = (target.targetVec - lpData.eyePos).GetAngles(true);

			if (target.id >= 0) {
				btTick = target.backTick;
				cmd->tick_count = TimeToTicks(track->GetLastItem(target.backTick).time[target.id] + Engine::LerpTime());

#ifdef PT_VISUALS
				if (btTick >= 0)
					Visuals::RenderPlayerCapsules(track->GetLastItem(btTick), Color(0.f, 1.f, 0.f, 1.f), target.id);
#endif
			}
		}

		if (btTick > BACKTRACK_TICKS)
			btTick = BACKTRACK_TICKS;

		if (target.id >= 0) {
			vec3_t dir, up, down;
			lpData.angles.GetVectors(dir, up, down, true);
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
	else if (Settings::aimbotSetViewAngles) {
		lpData.angles -= lpData.aimOffset - prevAimOffset;
	    engine->SetViewAngles(lpData.angles);
	} else {
		lpData.angles -= lpData.aimOffset;
	}

	prevAimOffset = lpData.aimOffset;

	aimbotTargets.Push(target);
}

static void ThreadedUpdate(UpdateData* data)
{
	MTR_SCOPED_TRACE("FwBridge", "ThreadedUpdate");

	for (int i : *data->updatedPlayers) {
		C_BasePlayer* ent = FwBridge::GetPlayerFast(data->players, i);
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
		if (!Engine::IsEnemy(ent))
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
	vecSoa<float, MULTIPOINT_COUNT, 3> tOffset(0);
	vecSoa<float, MULTIPOINT_COUNT, 3> tDir(0);

	int o = 0;

	vec3_t dir = hitboxes[idx]->bbmax - hitboxes[idx]->bbmin;
	dir.Normalize();

	vec3_t camCross = camDir[idx].Cross(dir);
	camCross.Normalize();

	vec3_t camCrossUp = camDir[idx].Cross(hUp[idx]).Cross(camDir[idx]);
	camCrossUp.Normalize();

	vec3_t camDirUp = camCross.Cross(camDir[idx]);

	camDirUp.Normalize();

	camCrossUp = camDirUp.LerpClamped(camCrossUp, camCrossUp.Dot(camDirUp) * 1.5f);
	camCrossUp.Normalize();

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

	//Make sure one (the first) point is in the middle - there is already enough overlap
	tOffset.AssignCol(0, (bbmax + bbmin) * 0.5f);

	hbList->mpOffset[idx] = tOffset.Rotate();
	auto rot = tDir.Rotate();
	hbList->mpDir[idx] = rot;
}

//The code below is single-threaded when it comes to updating the same player (obviously), but multithreaded when updating multiple worlds

struct BoneSetupData
{
	Players* players;
	int idx;
	bool output;

	BoneSetupData()
		: players(nullptr), idx(MAX_PLAYERS), output(false) {}

	BoneSetupData(Players* p, int id, bool out)
		: players(p), idx(id), output(out) {}
};

BoneSetupData boneSetupData[MAX_PLAYERS];

static void ThreadedBoneSetup(void* index)
{
	MTR_SCOPED_TRACE("FwBridge", "ThreadedBoneSetup");

	BoneSetupData* data = (BoneSetupData*)boneSetupData + (uintptr_t)index;

	data->output = Engine::UpdatePlayer(FwBridge::GetPlayerFast(*data->players, data->idx), data->players->bones[data->idx]);
}

static void UpdateHitboxesPart1(Players& __restrict players, const std::vector<int>* updatedPlayers, bool async)
{
	MTR_SCOPED_TRACE("FwBridge", "UpdateHitboxesPart1");

	for (int i : *updatedPlayers) {
		C_BasePlayer* ent = FwBridge::GetPlayerFast(players, i);

		studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());

		FwBridge::cachedHDRs[players.unsortIDs[i]] = hdr;

		if (!hdr)
			continue;

		mstudiohitboxset_t* set = hdr->GetHitboxSet(0);
		if (!set)
			continue;

		boneSetupData[players.unsortIDs[i]] = BoneSetupData(&players, i, false);

		Threading::QueueJobRef(ThreadedBoneSetup, (void*)(uintptr_t)players.unsortIDs[i]);
	}
}

static void UpdateHitboxesPart2(Players& __restrict players, const std::vector<int>* updatedPlayers, std::vector<int>* updatedHitboxPlayers)
{
	MTR_SCOPED_TRACE("FwBridge", "UpdateHitboxesPart2");

	for (int i = 0; i < MAX_PLAYERS; i++)
		if (boneSetupData[i].players == &players && boneSetupData[i].output) {
			updatedHitboxPlayers->push_back(players.sortIDs[i]);
			boneSetupData[i] = BoneSetupData();
		}

	for (int i : *updatedHitboxPlayers) {

		boneMatrix = players.bones[i];

		C_BasePlayer* ent = FwBridge::GetPlayerFast(players, i);
		studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());
		mstudiohitboxset_t* set = hdr->GetHitboxSet(0);

		HitboxList& hbList = players.hitboxes[i];
		int hb = -1;

		for (int idx = 0; idx < set->numhitboxes; idx++)
			FwBridge::hitboxIDs[idx] = -1;

		for (int idx = 0; idx < MAX_HITBOXES; idx++)
			FwBridge::reHitboxIDs[idx] = -1;

		for (int idx = 0; idx < set->numhitboxes && hb < MAX_HITBOXES - 1; idx++) {
			if (idx == Hitboxes::HITBOX_UPPER_CHEST ||
				idx == Hitboxes::HITBOX_LEFT_UPPER_ARM || idx == Hitboxes::HITBOX_RIGHT_UPPER_ARM) {
				continue;
			}

			hitboxes[++hb] = set->GetHitbox(idx);
			FwBridge::hitboxIDs[idx] = hb;
			FwBridge::reHitboxIDs[hb] = idx;

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
			  case HitGroups::HITGROUP_LEFTARM:
			  case HitGroups::HITGROUP_RIGHTARM:
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
	MTR_SCOPED_TRACE("FwBridge", "UpdateColliders");

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
		if (pID < prevPlayers.count && players.flags[i] & Flags::EXISTS && (~players.flags[i]) & Flags::UPDATED && prevPlayers.flags[pID] & Flags::UPDATED && FwBridge::GetPlayerFast(players, i) == FwBridge::GetPlayer(prevPlayers, pID)) {
			int fl = players.flags[i];
			players.flags[i] = prevPlayers.flags[pID];
			prevPlayers.flags[pID] = fl;
		}
	}
}
