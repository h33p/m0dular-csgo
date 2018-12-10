#include "resolver.h"
#include "../sdk/features/resolver.h"
#include "fw_bridge.h"
#include "engine.h"

enum ResolveBase
{
	BASE_FAKE = 0,
	BASE_MAX
};

static float resolveBases[MAX_PLAYERS][ResolveBase::BASE_MAX];
static float resolveOffsets[ResolveBase::BASE_MAX][2] = {{-180.f, 180.f}};
static RandomResolver groundResolver;
static RandomResolver inAirResolver;

float Resolver::resolvedAngles[MAX_PLAYERS];

void Resolver::Run(Players* __restrict players, Players* __restrict prevPlayers)
{
	bool isMoving[MAX_PLAYERS];
	int count = players->count;
	vec3_t atTarget = FwBridge::lpData.eyePos;

	groundResolver.UpdateOffsets(resolveOffsets);
	inAirResolver.UpdateOffsets(resolveOffsets);

	//Check if the player is moving, do a fakewalk check
	for (int i = 0; i < count; i++) {
		if (players->flags[i] & Flags::UPDATED) {
			int pID = players->unsortIDs[i];
			C_BasePlayer* ent = (C_BasePlayer*)players->instance[i];
			AnimationLayer* layers = ent->animationLayers();
			isMoving[pID] = (layers[4].weight != 0.0 || layers[6].cycle != 0.0 || layers[5].weight != 0.0) && ent->velocity().LengthSqr<2>() > 0.01f;
		}
	}

	//Calculate required resolver bases
	for (int i = 0; i < count; i++) {
		if (players->flags[i] & Flags::UPDATED) {
			int pID = players->unsortIDs[i];
			C_BasePlayer* ent = (C_BasePlayer*)players->instance[i];
			resolveBases[pID][BASE_FAKE] = ent->eyeAngles()[1];
		}
	}

	for (int i = 0; i < count; i++)
		if (players->flags[i] & Flags::UPDATED)
			groundResolver.UpdateBases(players->unsortIDs[i], resolveBases[players->unsortIDs[i]]);

	for (int i = 0; i < count; i++)
		if (players->flags[i] & Flags::UPDATED)
			inAirResolver.UpdateBases(players->unsortIDs[i], resolveBases[players->unsortIDs[i]]);

	//Resolve the players
	for (int i = 0; i < count; i++) {
		int rID = players->Resort(*prevPlayers, i);
		if (players->flags[i] & Flags::UPDATED) {
			int pID = players->unsortIDs[i];
			C_BasePlayer* ent = (C_BasePlayer*)players->instance[i];
			if (rID >= MAX_PLAYERS || players->time[i] - prevPlayers->time[rID] > globalVars->interval_per_tick * 2) {
				float targetAng = ent->eyeAngles()[1];

				if (ent->flags() & FL_ONGROUND) {
					if (isMoving[pID])
						targetAng = resolveBases[pID][BASE_FAKE];
					else
						targetAng = groundResolver.ResolvePlayer(pID);
				} else
					targetAng = inAirResolver.ResolvePlayer(pID);

				resolvedAngles[pID] = targetAng;
				(&ent->poseParameter())[11] = (180.f + (ent->eyeAngles()[1] - targetAng)) / 360.f;
				//ent->eyeAngles()[0] = 89.f;
			}
		}
	}
}

void Resolver::ShootPlayer(int target, bool onGround)
{
	if (onGround)
		groundResolver.ShotFired(target);
	else
		inAirResolver.ShotFired(target);
}

void Resolver::HitPlayer(int target, bool onGround, float angle)
{
	if (onGround)
		groundResolver.ProcessHit(target, angle);
	else
		inAirResolver.ProcessHit(target, angle);
}
