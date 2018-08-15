#include "resolver.h"
#include "../sdk/features/resolver.h"
#include "fw_bridge.h"
#include "engine.h"

enum ResolveBase
{
	BASE_LBY = 0,
	BASE_AT_TARGET,
	BASE_VELOCITY,
	BASE_STATIC,
	BASE_FAKE,
	BASE_FREESTANDING,
	BASE_LASTMOVINGLBY,
	BASE_MAX
};

static float resolveBases[MAX_PLAYERS][ResolveBase::BASE_MAX];
static RandomResolver groundResolver;
static RandomResolver inAirResolver;

float Resolver::resolvedAngles[MAX_PLAYERS];

void Resolver::Run(Players* __restrict players, Players* __restrict prevPlayers)
{
	bool isMoving[MAX_PLAYERS];
	int count = players->count;
	vec3_t atTarget = FwBridge::lpData.eyePos;

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
			resolveBases[pID][BASE_LBY] = ent->lowerBodyYawTarget();
			resolveBases[pID][BASE_AT_TARGET] = (atTarget - players->origin[i]).GetAngles(true)[1];
			resolveBases[pID][BASE_VELOCITY] = Engine::velocities[pID].GetAngles(true)[1];
			resolveBases[pID][BASE_STATIC] = 0.f;

			//TODO: freestanding calculation
			resolveBases[pID][BASE_FREESTANDING] = 0.f;

			if (isMoving[pID])
				resolveBases[pID][BASE_LASTMOVINGLBY] = ent->lowerBodyYawTarget();
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
		if (players->flags[i] & Flags::UPDATED) {
			int pID = players->unsortIDs[i];
			C_BasePlayer* ent = (C_BasePlayer*)players->instance[i];
			float targetAng = ent->eyeAngles()[1];

			if (ent->flags() & FL_ONGROUND) {
				if (isMoving[pID])
					targetAng = resolveBases[pID][BASE_LBY];
				else
					targetAng = groundResolver.ResolvePlayer(pID);
			} else
				targetAng = inAirResolver.ResolvePlayer(pID);

			resolvedAngles[pID] = targetAng;
			ent->eyeAngles()[1] = targetAng;
			ent->eyeAngles()[0] = 89.f;
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
