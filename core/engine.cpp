#include "engine.h"
#include "fw_bridge.h"
#include "settings.h"
#include "mtr_scoped.h"
#include "../features/resolver.h"
#include "../sdk/framework/utils/stackstring.h"
#include "../sdk/framework/math/mmath.h"
#include "../sdk/source_csgo/sdk.h"
#include <algorithm>

float dtime = 0;

static void InvalidateBoneCache(C_BasePlayer* ent)
{
	ent->lastOcclusionCheck() = globalVars->framecount;
	ent->occlusionFlags() = 0;
	ent->occlusionFlags2() = -1;
	ent->lastBoneTime() = globalVars->curtime - fmaxf(ent->simulationTime() - ent->prevSimulationTime(), globalVars->interval_per_tick);
	ent->mostRecentBoneCounter() = 0;
	ent->lastBoneFrameCount() = globalVars->framecount - 2;
	ent->prevBoneMask() = BONE_USED_BY_ANYTHING & ~BONE_USED_BY_HITBOX;
}

bool Engine::UpdatePlayer(C_BasePlayer* ent, matrix<3,4> matrix[128])
{
	MTR_SCOPED_TRACE("Engine", "UpdatePlayer");

	InvalidateBoneCache(ent);
	ent->varMapping().interpolatedEntries = 0;

	int flags = ent->effects();
	ent->effects() |= EF_NOINTERP;
	//TODO: Figure out a way not to mess up the hitboxes when not using BONE_USED_BY_ANYTHING
	bool ret = ent->SetupBones(matrix, MAXSTUDIOBONES, BONE_USED_BY_HITBOX | BONE_USED_BY_ANYTHING, globalVars->curtime + 10);

	/*ent->lastBoneTime() = globalVars->curtime - fmaxf(ent->simulationTime() - ent->prevSimulationTime(), globalVars->interval_per_tick);
	ent->mostRecentBoneCounter() = 0;
	ent->lastBoneFrameCount() = globalVars->framecount - 2;
	ent->prevBoneMask() = 0;*/

	//if (animState)
	//	animState->groundedFraction = fractionBackup;

	ent->effects() = flags;

	return ret;
}

static matrix<3,4> matrices[MAX_PLAYERS][128];
static bool bonesSetup[128];
static vec3 origins[MAX_PLAYERS];

void Engine::StartLagCompensation()
{
	Players& players = FwBridge::playerTrack.GetLastItem(0);
	for (int i = 0; i < players.count; i++) {
		int id = players.unsortIDs[i];

		C_BasePlayer* ent = FwBridge::GetPlayerFast(players, i);

		if (!ent)
			continue;

		studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());
		if (!hdr)
			continue;

		int bones = hdr->numbones;

		origins[i] = ent->GetClientRenderable()->GetRenderOrigin();

		CUtlVector<matrix3x4_t>& matrix = ent->boneMatrix();
		memcpy(matrices[id], matrix.memory.memory, sizeof(matrix3x4_t) * bones);
	}
}

void Engine::EndLagCompensation()
{
	Players& players = FwBridge::playerTrack.GetLastItem(0);
	for (int i = 0; i < players.count; i++) {
		int id = players.unsortIDs[i];

		C_BasePlayer* ent = FwBridge::GetPlayerFast(players, i);

		if (!ent)
			continue;

		studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());
		if (!hdr)
			continue;

		int bones = hdr->numbones;

		SetAbsOrigin(ent, origins[i]);

		CUtlVector<matrix3x4_t>& matrix = ent->boneMatrix();
		memcpy(matrix.memory.memory, matrices[id], sizeof(matrix3x4_t) * bones);
	}
}

matrix3x4_t Engine::GetDirectBone(C_BasePlayer* ent, studiohdr_t** hdr, size_t boneID)
{
	if (!*hdr)
		*hdr = mdlInfo->GetStudiomodel(ent->GetModel());

	if (!*hdr)
		return matrix3x4_t();

	int bones = (*hdr)->numbones;

	if (boneID > bones)
		return matrix3x4_t();

	CUtlVector<matrix3x4_t>& matrix = ent->boneMatrix();
	return matrix.memory.memory[boneID];
}

bool Engine::CopyBones(C_BasePlayer* ent, matrix3x4_t* matrix, int maxBones)
{
	if (!ent)
		return false;

	int entID = ent->EntIndex();

	if (!bonesSetup[entID])
		return false;

	if (!matrix)
		return true;

	studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());

	if (!hdr)
		return false;

	int bones = hdr->numbones;

	if (maxBones < bones || bones > 128)
		return false;

	memcpy(matrix, matrices[entID], sizeof(matrix3x4_t) * Min(maxBones, bones));

	return true;
}

/*
  Backup the server side animation layers.
  We will restore those later on. We set bClientSideAnimation to false,
  to make sure the game does not update them together with the server's anim layers.
*/

AnimationLayer serverAnimations[MAX_PLAYERS][13];
int prevFlags[MAX_PLAYERS];
vec3_t prevOrigins[MAX_PLAYERS];
vec3_t prevVelocities[MAX_PLAYERS];
vec3_t curVelocities[MAX_PLAYERS];
bool lastOnGround[MAX_PLAYERS];
float prevSimulationTime[MAX_PLAYERS];
bool prevVisible[MAX_PLAYERS];

int health[MAX_PLAYERS];

static uint64_t dirtyVisualBonesMask = ~0u;

void Engine::StartAnimationFix(Players* players, Players* prevPlayers)
{
    MTR_SCOPED_TRACE("Engine", "StartAnimationFix");
	size_t count = players->count;

	for (size_t i = 0; i < count; i++) {
		if (players->Resort(*prevPlayers, i) >= prevPlayers->count)
			continue;
		C_BasePlayer* ent = FwBridge::GetPlayerFast(*players, i);
		memcpy(serverAnimations[i], ent->animationLayers(), sizeof(AnimationLayer) * 13);
	}

	float curtime = globalVars->curtime;
	float frametime = globalVars->frametime;
	int framecount = globalVars->framecount;

	int pFlags[MAX_PLAYERS];

	for (size_t i = 0; i < count; i++) {
		if (players->flags[i] & Flags::UPDATED) {
			C_BasePlayer* ent = FwBridge::GetPlayerFast(*players, i);
			ent->clientSideAnimation() = false;

			int pID = players->unsortIDs[i];
			pFlags[i] = ent->flags();

			if (!prevVisible[pID]) {
				prevFlags[pID] = pFlags[i];
				lastOnGround[pID] = pFlags[i] & FL_ONGROUND;
			}

			//Predict the FL_ONGROUND flag.
			//lastOnGround deals with some artifacting (FL_ONGROUND repeating for a couple of ticks) happenning by those checks
			if (~pFlags[i] & FL_ONGROUND || ~prevFlags[pID] & FL_ONGROUND) {
				if (ent->animationLayers()[5].weight > 0.f && !lastOnGround[pID])
					ent->flags() |= FL_ONGROUND;
				else
					ent->flags() &= ~FL_ONGROUND;
			} else
				ent->flags() |= FL_ONGROUND;

			lastOnGround[pID] = ent->animationLayers()[5].weight > 0.f;

			if (ent->simulationTime() - prevSimulationTime[pID] > 0.f) {
				prevVelocities[pID] = curVelocities[pID];
				curVelocities[pID] = (players->origin[i] - prevOrigins[pID]) * (1.f / fmaxf(ent->simulationTime() - prevSimulationTime[pID], globalVars->interval_per_tick));
			}
		}
	}

	for (size_t i = 0; i < count; i++) {
		if (players->flags[i] & Flags::UPDATED) {
			C_BasePlayer* ent = FwBridge::GetPlayerFast(*players, i);
			CCSGOPlayerAnimState* animState = ent->animState();
			int pID = players->unsortIDs[i];

			if (health[pID] != ent->health()) {
				cvar->ConsoleDPrintf(ST("HP %d (-%d)\n"), ent->health(), health[pID] - ent->health());
				health[pID] = ent->health();
			}

			globalVars->curtime = ent->simulationTime();
			globalVars->framecount = animState->frameCount + 1;
			animState->updateTime = prevSimulationTime[pID]; //globalVars->curtime - globalVars->frametime * std::max(1, (int)((ent->simulationTime() - prevSimulationTime[pID]) / globalVars->interval_per_tick));

			int ticksToAnimate = (int)((ent->simulationTime() - prevSimulationTime[pID]) / globalVars->interval_per_tick);

			//cvar->ConsoleDPrintf("ANIM %d ticks\n", ticksToAnimate);
			for (int i = 0; i < ticksToAnimate; i++) {
				dirtyVisualBonesMask |= (1u << pID);
				float lerptime = (globalVars->curtime - prevSimulationTime[pID]) / (ent->simulationTime() - prevSimulationTime[pID]);
				vec3_t velocity = prevVelocities[pID].Lerp(curVelocities[pID], lerptime);
				SetAbsVelocity(ent, velocity);
				globalVars->frametime = globalVars->interval_per_tick;
				ent->UpdateClientSideAnimation();
				globalVars->curtime += globalVars->interval_per_tick;
				animState->groundedFraction = 0;
			}

			ent->angles()[1] = animState->goalFeetYaw;

			SetAbsAngles(ent, ent->angles());
			SetAbsOrigin(ent, ent->origin());
			//ent->flags() = pFlags[i];
			prevFlags[pID] = ent->flags();
			prevOrigins[pID] = players->origin[i];
			prevSimulationTime[pID] = ent->simulationTime();
		}
	}

	//Resolver overwrites the pose parameters, we do not want the animstate to change them back!
#ifdef TESTING_FEATURES
	if (Settings::resolver)
		Resolver::Run(players, prevPlayers);
#endif

	globalVars->curtime = curtime;
	globalVars->frametime = frametime;
	globalVars->framecount = framecount;

	memset(prevVisible, 0, sizeof(prevVisible));

	for (size_t i = 0; i < count; i++) {
		if (players->Resort(*prevPlayers, i) >= prevPlayers->count)
			continue;
		prevVisible[players->unsortIDs[i]] = true;
		C_BasePlayer* ent = FwBridge::GetPlayerFast(*players, i);
		memcpy(ent->animationLayers(), serverAnimations[i], sizeof(AnimationLayer) * 13);
	}
}

ConVar* bigUdRate = nullptr;
ConVar* minUdRate = nullptr;
ConVar* maxUdRate = nullptr;
ConVar* interpRatio = nullptr;
ConVar* clInterp = nullptr;
ConVar* minInterp = nullptr;
ConVar* maxInterp = nullptr;

static ConVar* gameType = nullptr;
static ConVar* gameMode = nullptr;

bool Engine::IsEnemy(C_BasePlayer* ent)
{
	if (!gameType)
		gameType = cvar->FindVar(ST("game_type"));

	if (!gameMode)
		gameMode = cvar->FindVar(ST("game_mode"));

	if (gameType->GetInt() == 6 && !gameMode->GetInt()) {
		if (FwBridge::localPlayer->survivalTeamNum() == -1)
			return true;
		return FwBridge::localPlayer->survivalTeamNum() ^ ent->survivalTeamNum();
	}

	return ent->teamNum() ^ FwBridge::localPlayer->teamNum();
}

vec3_t prevAimPunchAngles = vec3_t(0);

vec3_t Engine::PredictAimPunchAngle()
{
	if (!FwBridge::activeWeapon)
		return FwBridge::localPlayer->aimPunchAngle();

	float timeTillFire = FwBridge::activeWeapon->nextPrimaryAttack() - globalVars->curtime;
	int ticksTillFire = TimeToTicks(timeTillFire);

	vec3_t punchBackup = FwBridge::localPlayer->aimPunchAngle();

	if (ticksTillFire <= 0)
		prevAimPunchAngles = punchBackup;
	else
		for (int i = 0; i < ticksTillFire; i++)
			gameMovement->DecayAimPunchAngle();

	vec3_t ret = FwBridge::localPlayer->aimPunchAngle();

	FwBridge::localPlayer->aimPunchAngle() = punchBackup;

	if (FwBridge::weaponInfo) {
	    float fireRate = FwBridge::weaponInfo->flCycleTime();
		float lerpTime = 1.f - (timeTillFire / fireRate);
		//cvar->ConsoleDPrintf("LT: %f\n", lerpTime);
	    ret = prevAimPunchAngles.LerpClamped(ret, lerpTime);
	}

	return ret;
}

float Engine::LerpTime()
{

	if (!bigUdRate)
		bigUdRate = cvar->FindVar(ST("cl_updaterate"));
	if (!minUdRate)
		minUdRate = cvar->FindVar(ST("sv_minupdaterate"));
	if (!maxUdRate)
		maxUdRate = cvar->FindVar(ST("sv_maxupdaterate"));
	if (!interpRatio)
		interpRatio = cvar->FindVar(ST("cl_interp_ratio"));
	if (!clInterp)
		clInterp = cvar->FindVar(ST("cl_interp"));
	if (!minInterp)
		minInterp = cvar->FindVar(ST("sv_client_min_interp_ratio"));
	if (!maxInterp)
		maxInterp = cvar->FindVar(ST("sv_client_max_interp_ratio"));

	float updateRate = bigUdRate->GetFloat();

	if (minUdRate && maxUdRate)
		updateRate = std::clamp(updateRate, (float)(int)minUdRate->GetFloat(), (float)(int)maxUdRate->GetFloat());

	float ratio = interpRatio->GetFloat();

	float lerp = clInterp->GetFloat();

	if (minInterp && maxInterp && minInterp->GetFloat() != -1)
		ratio = std::clamp(ratio, minInterp->GetFloat(), maxInterp->GetFloat());
	else if (ratio == 0)
		ratio = 1.f;

	return std::max(lerp, ratio / updateRate);
}

ConVar* sv_maxunlag = nullptr;

float Engine::CalculateBacktrackTime()
{
	INetChannelInfo* nci = engine->GetNetChannelInfo();

	float correct = nci ? nci->GetLatency(FLOW_OUTGOING) + nci->GetLatency(FLOW_INCOMING) : 0.f;

	float lerpTime = LerpTime();
	float maxunlag = 1.f;

	if (!sv_maxunlag)
		sv_maxunlag = cvar->FindVar(ST("sv_maxunlag"));

	if (sv_maxunlag)
		maxunlag = sv_maxunlag->GetFloat();

	correct += lerpTime;
	correct = fmaxf(0.f, fminf(correct, maxunlag));

	return globalVars->curtime - correct;
}

void Engine::Shutdown()
{
	for (int i = 1; i < 64; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)entityList->GetClientEntity(i);

		if (!ent || !ent->IsPlayer())
			continue;

		ent->clientSideAnimation() = true;
		ent->varMapping().interpolatedEntries = ent->varMapping().entries.size;
	}
}

static void ValidateBoneCache(C_BasePlayer* ent)
{
	ent->lastBoneTime() = globalVars->curtime;
	ent->mostRecentBoneCounter() = *modelBoneCounter;
	ent->lastBoneFrameCount() = globalVars->framecount;
	ent->prevBoneMask() = BONE_USED_BY_ANYTHING;
}

static Players* lastLerpWorld[MAX_PLAYERS];
static float lastLerpTime[MAX_PLAYERS];
static vec3_t lerpedOrigin[MAX_PLAYERS];
static vec3_t lastLerpedOrigin[MAX_PLAYERS];

static void FrameUpdatePlayer(C_BasePlayer* ent)
{
	MTR_SCOPED_TRACE("Engine", "FrameUpdatePlayer");

	ent->mostRecentBoneCounter() = *modelBoneCounter - 1;
	int flags = ent->effects();
	ent->effects() |= EF_NOINTERP;

	int entID = ent->EntIndex();

	//TODO: Lerp the matrix to make it smooth
	/*Players* players1 = nullptr;
	Players* players2 = nullptr;
	Players* players3 = nullptr;

	for (size_t i = 0; i < FwBridge::playerTrack.Count(); i++) {
		Players& players = FwBridge::playerTrack[i];
		if (players.sortIDs[entID] >= 0 && players.sortIDs[entID] < players.count) {
			if (!players1)
				players1 = &players;
			else if (!players2)
				players2 = &players;
			else { //players3 is currently unused but using it would give even smoohter result
				players3 = &players;
				break;
			}
		}
	}

	if (!players1) {
		//lastLerpWorld[entID] = players1;
		return;
	}

	int pID1 = players1->sortIDs[entID];

	if (!lastLerpWorld[entID])
		lerpedOrigin[entID] = players1->origin[pID1];

	if (!lastLerpWorld[entID] || lastLerpWorld[entID] != players1) {
		lastLerpTime[entID] = globalVars->curtime;
		lastLerpWorld[entID] = players1;
		lastLerpedOrigin[entID] = lerpedOrigin[entID];
	}

	float timeDelta = globalVars->curtime - lastLerpTime[entID];

	if (players2) {
		int pID2 = players2->sortIDs[entID];
		float timeDeltaLerp = players1->time[pID1] - players2->time[pID2];
		lerpedOrigin[entID] = lastLerpedOrigin[entID].LerpClamped(players1->origin[pID1], timeDelta / timeDeltaLerp);
	} else
		lerpedOrigin[entID] = players1->origin[pID1];

		vec3_t originDelta = lerpedOrigin[entID] - players1->origin[pID1];*/

	//FIXME: We need to call SetupBones or else the rendered matrix will mess up
	if (dirtyVisualBonesMask & (1u << entID)) {
		InvalidateBoneCache(ent);
		ent->SetupBones(matrices[entID], MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, globalVars->curtime);
	}
	//memcpy(matrices[entID], players1->bones[pID1], MAXSTUDIOBONES);
	/*for (size_t i = 0; i < MAXSTUDIOBONES; i++) {
		matrices[entID][i].vec.AddRow(3, originDelta);
	}*/

	bonesSetup[entID] = true;
	//lerpedOrigin[entID] = players1->origin[pID1];
	ent->effects() = flags;
	ValidateBoneCache(ent);
}

static matrix3x4_t lastLPMatrix[128];
static matrix3x4_t firstLPMatrix[128];
static matrix3x4_t tempMatrix[128];
static vec3_t angleBackup;

static float localPoseParamBackup[24];
static AnimationLayer localAnimLayerBackup[13];

static void FrameUpdateLocalPlayer(C_BasePlayer* ent)
{
	MTR_SCOPED_TRACE("Engine", "FrameUpdateLocalPlayer");

#ifndef _WIN32
	if (!input->cameraInThirdPerson) {
		ent->clientSideAnimation() = true;
		ent->varMapping().interpolatedEntries = ent->varMapping().entries.size;
		return;
	}
#endif

	ent->lastOcclusionCheck() = globalVars->framecount;
	ent->occlusionFlags() = 0;
	ent->occlusionFlags2() = -1;
	ent->lastBoneTime() = globalVars->curtime - fmaxf(ent->simulationTime() - ent->prevSimulationTime(), globalVars->interval_per_tick);
	ent->mostRecentBoneCounter() = 0;
	ent->lastBoneFrameCount() = globalVars->framecount - 2;
	ent->prevBoneMask() = 0;

	int flags = ent->effects();
	ent->effects() |= EF_NOINTERP;

	CCSGOPlayerAnimState* state = ent->animState();
	if (state) {
		if (FwBridge::localPlayerSentPacket) {

			//We simulate animations twice. First we get the "fake" animation state by only feeding in the last angle and then the "real" one
			//CCSGOPlayerAnimState stateBackup = *ent->animState();

			float ctbac = globalVars->curtime;
			globalVars->curtime = (ent->tickBase() - FwBridge::localPlayerAngles.size()) * globalVars->interval_per_tick;

			for (const vec3_t& ang : FwBridge::localPlayerAngles) {
				globalVars->curtime += globalVars->interval_per_tick;

				//TODO: Predict LBY

				ent->localAngles() = ang;
				ent->eyeAngles() = ang;
				if (Settings::thirdPersonShowReal)
					ent->UpdateClientSideAnimation();
				angleBackup = ang;
			}
			if (!Settings::thirdPersonShowReal)
				ent->UpdateClientSideAnimation();

			FwBridge::localPlayerSentPacket = false;
			FwBridge::localPlayerAngles.clear();

			globalVars->curtime = ctbac;

			ent->angles()[0] = 0;
			ent->angles()[2] = 0;

			memcpy(localAnimLayerBackup, ent->animationLayers(), sizeof(AnimationLayer) * 13);
			memcpy(localPoseParamBackup, &ent->poseParameter(), sizeof(float) * 24);
		}
	}

	//FIXME: Disabling local player animations messes with viewmodel when jumping/crouching.
	ent->clientSideAnimation() = false;

	ent->localAngles() = vec3(0);
	ent->angles()[0] = 0;
	ent->angles()[2] = 0;

	SetAbsAngles(ent, ent->angles());
	ent->SetupBones(matrices[ent->EntIndex()], MAXSTUDIOBONES, BONE_USED_BY_ANYTHING & ~BONE_USED_BY_HITBOX, globalVars->curtime);
	bonesSetup[ent->EntIndex()] = true;

	if (false) {
		if (SourceFakelag::state & FakelagState::LAST)
			memcpy(lastLPMatrix, tempMatrix, sizeof(tempMatrix));
		if (SourceFakelag::state & FakelagState::FIRST)
			memcpy(firstLPMatrix, tempMatrix, sizeof(tempMatrix));
	}

	ent->effects() = flags;
	ValidateBoneCache(ent);
}

//Players could have changed in this state, let's just loop the engine entity list
void Engine::FrameUpdate()
{
	MTR_SCOPED_TRACE("Engine", "FrameUpdate");

	memset(bonesSetup, 0, sizeof(bonesSetup));

	for (int i = 1; i < 64; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)entityList->GetClientEntity(i);

		if (!ent)
			continue;

		bool player = ent->IsPlayer();
		bool dormant = ent->IsDormant();

		if (!ent || !player || dormant || i == 0)
			continue;

		playerCount = i;

		if (ent == FwBridge::localPlayer)
			continue;

		//if (~dirtyVisualBonesMask & (1u << i))
		//	continue;

		Threading::QueueJobRef(FrameUpdatePlayer, ent);
	}

	if ((Settings::thirdPerson
#ifdef TESTING_FEATURES
		 || Settings::headCam
#endif
			) && !input->CAM_IsThirdPerson(-1))
		input->CAM_ToThirdPerson();

	input->cameraOffset[2] = 0.f;

	Threading::FinishQueue(true);

	if (FwBridge::localPlayer) {
		FrameUpdateLocalPlayer(FwBridge::localPlayer);
		//TODO: Move these small features to a separate source file in the features directory
		if (Settings::noFlash)
			FwBridge::localPlayer->flashDuration() = 0.f;
	}

	dirtyVisualBonesMask = 0;
}

void RunSimulation(CPrediction* predictionPtr, float curtime, int command_number, CUserCmd* tCmd, C_BaseEntity* localPlayerEnt)
{
#ifdef _WIN32
	RunSimulationFunc(predictionPtr, nullptr, 0, 0, curtime, command_number, tCmd, localPlayerEnt);
#else
	RunSimulationFunc(predictionPtr, command_number, tCmd, localPlayerEnt, curtime);
#endif
}
