#include "awall.h"
#include "tracing.h"
#include "gametrace.h"
#include "fw_bridge.h"

bool IsBreakableEntity(C_BaseEntity* ent)
{
	return false;
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
	//TODO: change this to skip players once players pass is done
	//CTraceFilterSkipPlayers filter;
	CTraceFilter filter;

	*inBreakable = IsBreakableEntity((C_BaseEntity*)inTrace.ent);

	//TODO: Add hitbox handling pass
	int firstContents = engineTrace->GetPointContents(start + dirAdd, MASK_SHOT_HULL | CONTENTS_HITBOX, nullptr);

	for (float curDist = 0.f; curDist < maxDist; curDist += rayExtens) {
		start += dirAdd;

		int pointContents = engineTrace->GetPointContents(start, MASK_SHOT_HULL, nullptr);

		if ((pointContents & (MASK_SHOT_HULL | CONTENTS_HITBOX)) ^ MASK_SHOT_HULL && pointContents ^ firstContents) {
			end = start - dirAdd;

			lineRay.Init(start, end);
			Tracing2::GameTraceRay(lineRay, MASK_SHOT_HULL | CONTENTS_HITBOX, &filter, outTrace);

			//This is a friendly reminder that our TODO list includes hitbox handling to be put right below here


			if (outTrace->fraction < 0.99f && !outTrace->startsolid) {
				//NOTE: IsBreakableEntity check has to be done in a thread-safe way
				*outBreakable = IsBreakableEntity((C_BaseEntity*)outTrace->ent);

				if ((inBreakable && outBreakable) || inTrace.surface.flags & SURF_NODRAW || outTrace->plane.normal.Dot(dir) <= 1.f)
					return true;
			} else if (false && inTrace.ent != GameTrace::worldEnt && inBreakable) { //TODO: look into this
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
	if (penetrationPower <= 0.f || (!TraceToExitWorld(inTrace, outTrace, (const vec3_t)inTrace.endpos, dir, inBreakable, outBreakable) && ~engineTrace->GetPointContents(inTrace.endpos, MASK_SHOT_HULL, nullptr) & MASK_SHOT_HULL))
		return false;

	C_BasePlayer* ent = (C_BasePlayer*)inTrace.ent;

	bool isGrate = inTrace.contents & CONTENTS_GRATE;
	bool isNodraw = inTrace.surface.flags & SURF_NODRAW;

	surfacedata_t* inSurfData = physProp->GetSurfaceData(inTrace.surface.surfaceProps);
	int inMat = inSurfData->game.material;
	float inPenMod = inSurfData->game.flPenetrationModifier;
	//float inDmgMod = inSurfData->game.flDamageModifier;

	surfacedata_t* outSurfData = physProp->GetSurfaceData(outTrace->surface.surfaceProps);
	int outMat = outSurfData->game.material;
	float outPenMod = outSurfData->game.flPenetrationModifier;
	//float outDmgMod = outSurfData->game.flDamageModifier;

	//Part of the newer penetration system
	float finalDmgMod = 0.16f;
	float combinedPenMod = 3;

	//We could really just build a table for branchless modifier access
	if (inMat == CHAR_TEX_GRATE || inMat == CHAR_TEX_GLASS)
		finalDmgMod = 0.05f;
	else if (isGrate || isNodraw)
		combinedPenMod = 1;
	else if (inMat == CHAR_TEX_FLESH && !FwBridge::IsEnemy(ent) && !ff_damage_reduction_bullets) {
		//TODO: This branch will never be used in the world trace pass, handle it in the players pass
		if (!ff_damage_bullet_penetration)
			return false;
		combinedPenMod = ff_damage_bullet_penetration;
	} else if (inMat == outMat && outMat == CHAR_TEX_PLASTIC)
		combinedPenMod = 2;
	else
		combinedPenMod = (inPenMod + outPenMod) * 0.5f;

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

	trace_t inTrace;
	trace_t outTrace;
	//TODO: Players pass
	CTraceFilter filter;
	filter.pSkip = FwBridge::localPlayer;
	Ray_t lineRay;

	float curDamage = weaponDamage;
	float curRange = weaponRange;
	float curDistance = 0.f;

	float penDist = 3000.f;

	bool hbp = true;

	for (int hitsRemaining = MAX_INTERSECTS - 1; hitsRemaining >= 0 && curDamage && hbp; hitsRemaining--) {
		//NOTE: xAE's code had this wrong. But it could also be Valve so you never know
		curRange = weaponRange - curDistance;

		vec3_t end = start + dir * curRange;
		lineRay.Init(start, end);
		Tracing2::GameTraceRay(lineRay, MASK_SHOT_HULL | CONTENTS_HITBOX, &filter, &inTrace);
		//Here has to go ClipTraceToPlayers, but we have to do it in the TODO player pass

		surfacedata_t* inSurfData = physProp->GetSurfaceData(inTrace.surface.surfaceProps);
		float inPenMod = inSurfData->game.flPenetrationModifier;

		curDistance += inTrace.fraction * curRange;
		curDamage *= powf(weaponRangeModifier, curDistance * 0.002f);

		outDamages[*curOutID] = *curOutID ? outDamages[*curOutID - 1] - curDamage : weaponDamage - curDamage;
		outTraces[*curOutID] = inTrace;
		permaCache[(*curOutID)++] = true;

		if (inTrace.fraction == 1.f || inPenMod < 0.1f || (curDistance > penDist && weaponPenetration > 0.f))
			break;

		//TODO: Here have to be the team damage checks etc

		bool inBreakable = false;
		bool outBreakable = false;

		hbp = HandleBulletPenetrationWorld(inTrace, dir, !hitsRemaining, weaponPenetration, 1, ff_damage_reduction_bullets, ff_damage_bullet_penetration, &inBreakable, &outBreakable, &curDamage, &outTrace);

		permaCache[*curOutID - 1] = !inBreakable;

		outDamages[*curOutID] = outDamages[*curOutID - 1] - curDamage;
		outTraces[*curOutID] = outTrace;
		permaCache[(*curOutID)++] = !outBreakable;
	}
}

//This is the second (player) pass of autowall. Everything performed here is not cacheable

void AutoWall::FireBulletPlayers(vec3_t start, vec3_t dir, float weaponRange, float weaponRangeModifier, float weaponDamage, float weaponPenetration, int cacheSize, const bool* permaCache, const trace_t* inTraces, const float* inDamages)
{

}
