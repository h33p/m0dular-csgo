#include "engine.h"
#include "fw_bridge.h"
#include "../sdk/framework/math/mmath.h"
#include "../sdk/source_csgo/sdk.h"
#include <algorithm>

bool Engine::UpdatePlayer(C_BasePlayer* ent, matrix<3,4> matrix[128])
{
	*(int*)((uintptr_t)ent + x64x32(0xFEC, 0xA30)) = globalVars->framecount;
	*(int*)((uintptr_t)ent + x64x32(0xFE4, 0xA28)) = 0;
	*(unsigned long*)((uintptr_t)ent + x64x32(0x2C48, 0x2680)) = 0;

	ent->m_varMapping().m_nInterpolatedEntries = 0;

	int flags = ent->m_fFlags();
	ent->m_fFlags() |= EF_NOINTERP;
	bool ret = ent->SetupBones(matrix, MAXSTUDIOBONES, BONE_USED_BY_HITBOX, globalVars->curtime);
	ent->m_fFlags() = flags;

	if (!ret)
		return false;

	return true;
}

static matrix<3,4> matrices[MAX_PLAYERS][128];
static vec3 origins[MAX_PLAYERS];

int Engine::numBones[MAX_PLAYERS];

void Engine::StartLagCompensation()
{
	Players& players = FwBridge::playerTrack.GetLastItem(0);
	for (int i = 0; i < players.count; i++) {
		int id = players.unsortIDs[i];

		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];

		studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());
		if (!hdr) {
			numBones[id] = 0;
			continue;
		}

		int bones = hdr->numbones;
		numBones[id] = bones;

		origins[i] = ent->GetClientRenderable()->GetRenderOrigin();

		CUtlVector<matrix3x4_t>& matrix = ent->m_nBoneMatrix();
		memcpy(matrices[id], matrix.m_Memory.m_pMemory, sizeof(matrix3x4_t) * bones);
	}
}

void Engine::EndLagCompensation()
{
	Players& players = FwBridge::playerTrack.GetLastItem(0);
	for (int i = 0; i < players.count; i++) {
		int id = players.unsortIDs[i];

		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];

		studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());
		if (!hdr)
			continue;

		int bones = hdr->numbones;

		SetAbsOrigin(ent, origins[i]);

		CUtlVector<matrix3x4_t>& matrix = ent->m_nBoneMatrix();
		memcpy(matrix.m_Memory.m_pMemory, matrices[id], sizeof(matrix3x4_t) * bones);
	}
}

/*
  Backup the server side animation layers.
  We will restore those later on. We set bClientSideAnimation to false,
  to make sure the game does not update them together with the server's anim layers.
*/

static AnimationLayer serverAnimations[MAX_PLAYERS][13];
static int prevFlags[MAX_PLAYERS];
static vec3_t prevOrigins[MAX_PLAYERS];
static bool lastOnGround[MAX_PLAYERS];
static vec3 vel;
static float prevSimulationTime[MAX_PLAYERS];

void Engine::StartAnimationFix(Players* players, Players* prevPlayers)
{
	size_t count = players->count;

	for (size_t i = 0; i < count; i++) {
		if (prevPlayers->sortIDs[players->unsortIDs[i]] >= prevPlayers->count)
			continue;
		C_BasePlayer* ent = (C_BasePlayer*)players->instance[i];
		memcpy(serverAnimations[i], ent->m_pAnimationLayers(), sizeof(AnimationLayer) * 13);
	}

	float curtime = globalVars->curtime;
	float frametime = globalVars->frametime;
	int framecount = globalVars->framecount;

	for (size_t i = 0; i < count; i++) {
		if (players->flags[i] & Flags::UPDATED) {
			C_BasePlayer* ent = (C_BasePlayer*)players->instance[i];
			CCSGOPlayerAnimState* animState = ent->m_pAnimState();
			ent->m_bClientSideAnimation() = true;

			globalVars->curtime = ent->m_flPrevSimulationTime() + globalVars->interval_per_tick;
			globalVars->frametime = globalVars->interval_per_tick;
			globalVars->framecount = animState->frameCount() + 1;

			int pID = players->unsortIDs[i];
			int pFlags = ent->m_fFlags();

			//Predict the FL_ONGROUND flag.
			//lastOnGround deals with some artifacting (FL_ONGROUND repeating for a couple of ticks) happenning by those checks
			if (~pFlags & FL_ONGROUND || ~prevFlags[pID] & FL_ONGROUND) {
				if (ent->m_pAnimationLayers()[5].m_flWeight > 0.f && !lastOnGround[pID])
					ent->m_fFlags() |= FL_ONGROUND;
				else
					ent->m_fFlags() &= ~FL_ONGROUND;
			} else
				ent->m_fFlags() |= FL_ONGROUND;

			lastOnGround[pID] = ent->m_pAnimationLayers()[5].m_flWeight > 0.f;

			vel = (players->origin[i] - prevOrigins[pID]) * (1.f / (ent->m_flSimulationTime() - prevSimulationTime[pID]));

			SetAbsVelocity(ent, vel);

			//Here we will resolve the player
			//ent->m_angEyeAngles()[1] = ent->m_flLowerBodyYawTarget();

			ent->m_vecAngles()[1] = ent->m_angEyeAngles()[1];
			SetAbsAngles(ent, ent->m_vecAngles());

			ent->UpdateClientSideAnimation();
			ent->m_bClientSideAnimation() = false;
			ent->m_fFlags() = pFlags;
			prevFlags[pID] = ent->m_fFlags();
			prevOrigins[pID] = players->origin[i];
			prevSimulationTime[pID] = ent->m_flSimulationTime();
		}
	}

	globalVars->curtime = curtime;
	globalVars->frametime = frametime;
	globalVars->framecount = framecount;
}

void Engine::EndAnimationFix(Players* players, Players* prevPlayers)
{
	size_t count = players->count;

	for (size_t i = 0; i < count; i++) {
		if (prevPlayers->sortIDs[players->unsortIDs[i]] >= prevPlayers->count)
			continue;
		C_BasePlayer* ent = (C_BasePlayer*)players->instance[i];
		memcpy(ent->m_pAnimationLayers(), serverAnimations[i], sizeof(AnimationLayer) * 13);
	}
}

static ConVar* bigUdRate = nullptr;
static ConVar* minUdRate = nullptr;
static ConVar* maxUdRate = nullptr;
static ConVar* interpRatio = nullptr;
static ConVar* clInterp = nullptr;
static ConVar* minInterp = nullptr;
static ConVar* maxInterp = nullptr;

float Engine::LerpTime()
{
	if (!bigUdRate)
		bigUdRate = cvar->FindVar(("cl_updaterate"));
	if (!minUdRate)
		minUdRate = cvar->FindVar(("sv_minupdaterate"));
	if (!maxUdRate)
		maxUdRate = cvar->FindVar(("sv_maxupdaterate"));
	if (!interpRatio)
		interpRatio = cvar->FindVar(("cl_interp_ratio"));
	if (!clInterp)
		clInterp = cvar->FindVar(("cl_interp"));
	if (!minInterp)
		minInterp = cvar->FindVar(("sv_client_min_interp_ratio"));
	if (!maxInterp)
		maxInterp = cvar->FindVar(("sv_client_max_interp_ratio"));

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

float Engine::CalculateBacktrackTime()
{
	INetChannelInfo* nci = engine->GetNetChannelInfo();

	float correct = nci ? nci->GetLatency(FLOW_OUTGOING) + nci->GetLatency(FLOW_INCOMING) : 0.f;

	float lerpTime = LerpTime();

	correct += lerpTime;
	correct = fmaxf(0.f, fminf(correct, 1.f));

	return globalVars->curtime - 0.2f - correct;
}

void Engine::Shutdown()
{

	for (int i = 1; i < 64; i++)
	{
		C_BasePlayer* ent = (C_BasePlayer*)entityList->GetClientEntity(i);

		if (ent == FwBridge::localPlayer)
			continue;

		if (!ent || !ent->IsPlayer() || i == 0)
			continue;

		ent->m_bClientSideAnimation() = true;
		ent->m_varMapping().m_nInterpolatedEntries = ent->m_varMapping().m_Entries.m_Size;
	}
}
