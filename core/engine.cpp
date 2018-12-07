#include "engine.h"
#include "fw_bridge.h"
#include "resolver.h"
#include "../sdk/framework/utils/stackstring.h"
#include "../sdk/framework/math/mmath.h"
#include "../sdk/source_csgo/sdk.h"
#include <algorithm>

bool Engine::UpdatePlayer(C_BasePlayer* ent, matrix<3,4> matrix[128])
{
	//TODO: put these to the player class
	*(int*)((uintptr_t)ent + x64x32(0xFEC, 0xA30)) = globalVars->framecount;
	*(int*)((uintptr_t)ent + x64x32(0xFE4, 0xA28)) = 0;
	*(int*)((uintptr_t)ent + x64x32(0xFE0, 0xA24)) = -1;
	ent->lastBoneTime() = globalVars->curtime - fmaxf(ent->simulationTime() - ent->prevSimulationTime(), globalVars->interval_per_tick);
	*(uintptr_t*)((uintptr_t)ent + x64x32(0x2C48, 0x2680)) = 0;

	ent->lastBoneFrameCount() = globalVars->framecount - 2;
	ent->prevBoneMask() = 0;

	ent->varMapping().interpolatedEntries = 0;

	CCSGOPlayerAnimState* animState = ent->animState();

	float fractionBackup = 0;
	if (animState) {
		fractionBackup = animState->groundedFraction;
		animState->groundedFraction = 0;
	}

	int flags = ent->effects();
	ent->effects() |= EF_NOINTERP;
	bool ret = ent->SetupBones(matrix, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, globalVars->curtime + 10);

	if (animState)
		animState->groundedFraction = fractionBackup;

	ent->effects() = flags;

	return ret;
}

matrix<3,4> matrices[MAX_PLAYERS][128];
vec3 origins[MAX_PLAYERS];

int Engine::numBones[MAX_PLAYERS];
vec3_t Engine::velocities[MAX_PLAYERS];

void Engine::StartLagCompensation()
{
	Players& players = FwBridge::playerTrack.GetLastItem(0);
	for (int i = 0; i < players.count; i++) {
		int id = players.unsortIDs[i];

		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];

		if (!ent)
			continue;

		studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());
		if (!hdr) {
			numBones[id] = 0;
			continue;
		}

		int bones = hdr->numbones;
		numBones[id] = bones;

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

		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];

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

/*
  Backup the server side animation layers.
  We will restore those later on. We set bClientSideAnimation to false,
  to make sure the game does not update them together with the server's anim layers.
*/

AnimationLayer serverAnimations[MAX_PLAYERS][13];
int prevFlags[MAX_PLAYERS];
vec3_t prevOrigins[MAX_PLAYERS];
bool lastOnGround[MAX_PLAYERS];
float prevSimulationTime[MAX_PLAYERS];

void Engine::StartAnimationFix(Players* players, Players* prevPlayers)
{
	size_t count = players->count;

	for (size_t i = 0; i < count; i++) {
		if (players->Resort(*prevPlayers, i) >= prevPlayers->count)
			continue;
		C_BasePlayer* ent = (C_BasePlayer*)players->instance[i];
		memcpy(serverAnimations[i], ent->animationLayers(), sizeof(AnimationLayer) * 13);
	}

	float curtime = globalVars->curtime;
	float frametime = globalVars->frametime;
	int framecount = globalVars->framecount;

	int pFlags[MAX_PLAYERS];

	for (size_t i = 0; i < count; i++) {
		if (players->flags[i] & Flags::UPDATED) {
			C_BasePlayer* ent = (C_BasePlayer*)players->instance[i];
			ent->clientSideAnimation() = true;

			int pID = players->unsortIDs[i];
			pFlags[i] = ent->flags();

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

			if (ent->simulationTime() - prevSimulationTime[pID] > 0.f)
				velocities[pID] = (players->origin[i] - prevOrigins[pID]) * (1.f / fmaxf(ent->simulationTime() - prevSimulationTime[pID], globalVars->interval_per_tick));
			SetAbsVelocity(ent, velocities[pID]);
		}
	}

	//Resolver::Run(players, prevPlayers);

	for (size_t i = 0; i < count; i++) {
		if (players->flags[i] & Flags::UPDATED) {
			C_BasePlayer* ent = (C_BasePlayer*)players->instance[i];
			CCSGOPlayerAnimState* animState = ent->animState();
			int pID = players->unsortIDs[i];

			globalVars->curtime = ent->simulationTime();
			globalVars->frametime = globalVars->interval_per_tick;
			globalVars->framecount = animState->frameCount + 1;
			animState->updateTime = prevSimulationTime[pID]; //globalVars->curtime - globalVars->frametime * std::max(1, (int)((ent->simulationTime() - prevSimulationTime[pID]) / globalVars->interval_per_tick));

			ent->UpdateClientSideAnimation();

			ent->angles()[1] = animState->goalFeetYaw;
			SetAbsAngles(ent, ent->angles());
			SetAbsOrigin(ent, ent->origin());
			ent->clientSideAnimation() = false;
			ent->flags() = pFlags[i];
			prevFlags[pID] = ent->flags();
			prevOrigins[pID] = players->origin[i];
			prevSimulationTime[pID] = ent->simulationTime();
		}
	}

	globalVars->curtime = curtime;
	globalVars->frametime = frametime;
	globalVars->framecount = framecount;

	for (size_t i = 0; i < count; i++) {
		if (players->Resort(*prevPlayers, i) >= prevPlayers->count)
			continue;
		C_BasePlayer* ent = (C_BasePlayer*)players->instance[i];
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

		if (ent == FwBridge::localPlayer)
			continue;

		if (!ent || !ent->IsPlayer() || i == 0)
			continue;

		ent->clientSideAnimation() = true;
		ent->varMapping().interpolatedEntries = ent->varMapping().entries.size;
	}
}

void RunSimulation(CPrediction* prediction, float curtime, int command_number, CUserCmd* tCmd, C_BaseEntity* localPlayer)
{
#ifdef _WIN32
	RunSimulationFunc(prediction, nullptr, 0, 0, curtime, command_number, tCmd, localPlayer);
#else
	RunSimulationFunc(prediction, curtime, command_number, tCmd, localPlayer);
#endif
}
