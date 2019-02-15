#include "awall.h"
#include "../core/tracing.h"
#include "../sdk/features/gametrace.h"
#include "../core/fw_bridge.h"

Mutex beLock;

bool IsBreakableEntity(C_BasePlayer* ent)
{
	if (!ent || !ent->EntIndex())
		return false;

	int oldTakeDamage = 0;
	ClientClass* entClass = ent->GetClientClass();
	bool locked = false;

	crcs_t crc = Crc32(entClass->networkName);

	switch(crc) {
	  case "CBrekableSurface"_crc32:
	  case "CBaseDoor"_crc32:
	  case "CBaseEntity"_crc32:
		  beLock.lock();
		  oldTakeDamage = ent->takeDamage();
		  ent->takeDamage() = DAMAGE_YES;
		  locked = true;
		  break;
	}

	bool breakable = IsBreakableEntityNative(ent);

	if (locked) {
	    ent->takeDamage() = oldTakeDamage;
		beLock.unlock();
	}

	return breakable;
}

//This is a so called first pass of autowall. Players are ignored so as to be able to cache the results.
//TODO: Deal with breakable entities getting cached
//TODO: Add players pass

//NOTE: Needs a separate code path for obselete penetration system, but it never gets fired so consider having a completely separate autowall system to have just a single predictable branch

bool AutoWall::TraceToExitWorld(const trace_t& __restrict inTrace, trace_t* __restrict outTrace, vec3_t startPos, vec3_t dir, bool* inBreakable, bool* outBreakable)
{
	static constexpr float maxDist = 90.f, rayExtens = 4.f;

	vec3_t dirAdd = dir * rayExtens;
	vec3_t start = startPos;
	vec3_t end = start;
	Ray_t lineRay;
	CTraceFilterSkipPlayers filter;

	*inBreakable = IsBreakableEntity((C_BasePlayer*)inTrace.ent);

	//TODO: Add hitbox handling pass

	for (float curDist = 0.f; curDist < maxDist; curDist += rayExtens) {
		start += dirAdd;

		int pointContents = Tracing2::GetPointContents(start, MASK_SHOT_HULL, nullptr);

		if (~pointContents & MASK_SHOT_HULL) {
			end = start - dirAdd;

			lineRay.Init(start, end);
			Tracing2::GameTraceRay(lineRay, MASK_SHOT_HULL, &filter, outTrace);

			//This is a friendly reminder that our TODO list includes hitbox handling to be put right below here

			if (outTrace->DidHit() && !outTrace->startsolid) {
				//NOTE: IsBreakableEntity check has to be done in a thread-safe way
				*outBreakable = IsBreakableEntity((C_BasePlayer*)outTrace->ent);

				if ((*inBreakable && *outBreakable) || inTrace.surface.flags & SURF_NODRAW || (~outTrace->surface.flags & SURF_NODRAW && outTrace->plane.normal.Dot(dir) <= 1.f))
					return true;
			} else if (false && inTrace.ent != GameTrace::worldEnt && *inBreakable) { //TODO: look into this
			    *outTrace = inTrace;
				outTrace->endpos = start + dir;
				return true;
			}
		}
	}

	return false;
}

bool AutoWall::HandleBulletPenetrationWorld(const trace_t& inTrace, vec3_t dir, bool lastHit, float penetrationPower, bool sv_penetration_type, float ff_damage_reduction_bullets, float ff_damage_bullet_penetration, bool* inBreakable, bool* outBreakable, float* curDamage, trace_t* outTrace)
{
	//TODO: add handling of spetial Cache surface materials
	if (penetrationPower <= 0.f  || (!TraceToExitWorld(inTrace, outTrace, (const vec3_t)inTrace.endpos, dir, inBreakable, outBreakable) && ~Tracing2::GetPointContents(inTrace.endpos, MASK_SHOT_HULL, nullptr) & MASK_SHOT_HULL)) {
		*curDamage = 0;
		return false;
	}

	C_BasePlayer* ent = (C_BasePlayer*)inTrace.ent;

	bool isGrate = inTrace.contents & CONTENTS_GRATE;
	bool isNodraw = inTrace.surface.flags & SURF_NODRAW;

	surfacedata_t* inSurfData = physProp->GetSurfaceData(inTrace.surface.surfaceProps);
	int inMat = inSurfData->game.material;
	float inPenMod = inSurfData->game.flPenetrationModifier;

	surfacedata_t* outSurfData = physProp->GetSurfaceData(outTrace->surface.surfaceProps);
	int outMat = outSurfData->game.material;
	float outPenMod = outSurfData->game.flPenetrationModifier;

	//Part of the newer penetration system
	float finalDmgMod = 0.16f;
	float combinedPenMod = 3;

	//We could really just build a table for branchless modifier access
	if (inMat == CHAR_TEX_GRATE || inMat == CHAR_TEX_GLASS)
		finalDmgMod = 0.05f;
	else if (isGrate || isNodraw)
		combinedPenMod = 1;
	else if (inMat == CHAR_TEX_FLESH && !Engine::IsEnemy(ent) && !ff_damage_reduction_bullets) {
		//TODO: This branch will never be used in the world trace pass, handle it in the players pass
		if (!ff_damage_bullet_penetration)
			return false;
		combinedPenMod = ff_damage_bullet_penetration;
	} else
		combinedPenMod = (inPenMod + outPenMod) * 0.5f;

	if (inMat == outMat) {
		if (outMat == CHAR_TEX_PLASTIC)
			combinedPenMod = 2;
		else if (outMat == CHAR_TEX_CARDBOARD || outMat == CHAR_TEX_WOOD)
			combinedPenMod = 3;
	}

	float thickness = inTrace.endpos.DistToSqr(outTrace->endpos);
	float invPenMod = fmaxf(1.f / combinedPenMod, 0.f);

	float lostDmg = fmaxf((invPenMod * thickness * (1.f / 24.f)) + ((*curDamage * finalDmgMod) + fmaxf(3.75f / penetrationPower, 0.f) * 3.f * invPenMod), 0.f);

	*curDamage -= lostDmg;

	return *curDamage > 1.f;
}

void AutoWall::FireBulletWorld(vec3_t start, vec3_t dir, float weaponRange, float weaponRangeModifier, float weaponDamage, float weaponPenetration, int* curOutID, bool* permaCache, trace_t* outTraces, float* outDamages)
{

	//TODO: Do this through convars
	static constexpr float ff_damage_reduction_bullets = 0.1f;
	static constexpr float ff_damage_bullet_penetration = 0.f;

	vec3_t curStart = start;

	trace_t inTrace;
	trace_t outTrace;
	CTraceFilterSkipPlayers filter;
	Ray_t lineRay;

	float curDamage = weaponDamage;
	float curRange = weaponRange;
	float curDistance = 0.f;
	float prevDamage = 0.f;

	float penDist = 3000.f;

	bool hbp = true;

	for (int hitsRemaining = MAX_INTERSECTS - 1; hitsRemaining >= 0 && curDamage && hbp; hitsRemaining--) {
		//NOTE: xAE's code had this wrong. But it could also be Valve so you never know
		curRange = weaponRange - curDistance;

		vec3_t end = curStart + dir * curRange;
		lineRay.Init(curStart, end);
		Tracing2::GameTraceRay(lineRay, MASK_SHOT_HULL | CONTENTS_HITBOX, &filter, &inTrace);
		//Here has to go ClipTraceToPlayers, but we have to do it in the TODO player pass

		surfacedata_t* inSurfData = physProp->GetSurfaceData(inTrace.surface.surfaceProps);
		float inPenMod = inSurfData->game.flPenetrationModifier;

		prevDamage = curDamage;
		curDistance += inTrace.fraction * curRange;
		curDamage *= powf(weaponRangeModifier, curDistance * 0.002f);

		outDamages[*curOutID] = prevDamage - curDamage;
		outTraces[*curOutID] = inTrace;
		permaCache[(*curOutID)++] = true;

		if (inTrace.fraction == 1.f || inPenMod < 0.1f || (curDistance > penDist && weaponPenetration > 0.f))
			break;

		//TODO: Here have to be the team damage checks etc

		bool inBreakable = false;
		bool outBreakable = false;

		prevDamage = curDamage;
		hbp = HandleBulletPenetrationWorld(inTrace, dir, !hitsRemaining, weaponPenetration, 1, ff_damage_reduction_bullets, ff_damage_bullet_penetration, &inBreakable, &outBreakable, &curDamage, &outTrace);

		permaCache[*curOutID - 1] = !inBreakable;

		outDamages[*curOutID] = prevDamage - curDamage;
		outTraces[*curOutID] = outTrace;
		permaCache[(*curOutID)++] = !outBreakable;

		curStart = outTrace.endpos;
	}
}

//This is the second (player) pass of autowall. Everything performed here is not cacheable
static bool PerformPlayerClipping(Players* players, const trace_t& inTrace, float* curDistance, float* curDamage, int* hitsRemaining, uint64_t* ignoreFlags, float weaponRangeModifier, float weaponArmorPenetration, float damageDelta, float distanceDelta)
{
	//TODO: Un-recursify this. Recursion was mainly used because this will not be called for more than 4 times but it is not the most efficient regardless
	trace_t tempTrace = inTrace;
	tempTrace.ent = nullptr;
	int ret = Tracing2::ClipTraceToPlayers(&tempTrace, players, *ignoreFlags);

	if (ret >= 0 && ret < MAX_PLAYERS) {
		*ignoreFlags |= (1ull << ret);

		float newDistanceDelta = tempTrace.startpos.DistTo(tempTrace.endpos);
		distanceDelta -= newDistanceDelta;
		*curDistance += newDistanceDelta;


		float newDamage = *curDamage * powf(weaponRangeModifier, *curDistance * 0.002f);
		damageDelta -= *curDamage - newDamage;
		*curDamage = newDamage;

		(*hitsRemaining)--;

		//We hit an enemy player
		if (tempTrace.hitgroup != HitGroups::HITGROUP_GEAR &&
			tempTrace.hitgroup != HitGroups::HITGROUP_GENERIC &&
			Engine::IsEnemy((C_BasePlayer*)tempTrace.ent)) {
			*curDamage = Tracing2::ScaleDamage(players, players->sortIDs[ret], *curDamage, weaponArmorPenetration, FwBridge::hitboxIDs[tempTrace.hitbox], tempTrace.hitgroup);

			return true;
		}

		if (*hitsRemaining > 0) {
			trace_t newTrace = inTrace;
			newTrace.startpos = tempTrace.endpos;
			return PerformPlayerClipping(players, newTrace, curDistance, curDamage, hitsRemaining, ignoreFlags, weaponRangeModifier, weaponArmorPenetration, damageDelta, distanceDelta);
		}
	} else {
		*curDamage -= damageDelta;
		*curDistance += distanceDelta;
	}

	return false;
}

float AutoWall::FireBulletPlayers(vec3_t start, vec3_t dir, float weaponRange, float weaponRangeModifier, float weaponDamage, float weaponPenetration, float weaponArmorPenetration, int* cacheSize, const bool* permaCache, const trace_t* inTraces, const float* inDamages, Players* players)
{
	int hitsRemaining = MAX_INTERSECTS;

	//All even index inTraces will be in-air traces while odd ones - between the walls
	//First we need to get the intersection points with the players and inject those points in the loop
	int curID = 0;
	uint64_t ignoreFlags = 0;
	float curDamage = weaponDamage;
	float curDistance = 0;

	//The first pass is to go through the cache and check the non-permanent (breakable entity) entries to see if they were broken or not. If they were - we can fix up the cache so we do not have to repeat the work again
	/*for (int i = 0; i < *cacheSize; i++) {
		if (!permaCache[i]) {
			vec3_t start = inTraces[i].startpos;
			while (!permaCache[i++]);
			vec3_t end = inTraces[i].endpos;
		}
	}*/

	while (hitsRemaining > 0 && curID < *cacheSize) {
		trace_t inTrace = inTraces[curID];

		if (PerformPlayerClipping(players, inTrace, &curDistance, &curDamage, &hitsRemaining, &ignoreFlags, weaponRangeModifier, weaponArmorPenetration, inDamages[curID], inTraces[curID].startpos.DistTo(inTraces[curID].endpos))) {
			return curDamage;
		}

		curID++;

		//HandleBulletPenetration shrunk into 2 LOC
		curDamage -= inDamages[curID];
		curDistance += inTraces[curID].startpos.DistTo(inTraces[curID].endpos);
		curID++;

		hitsRemaining--;
	}

	return 0.f;
}
