#include "tracing.h"
#include "hooks.h"
#include "../sdk/framework/utils/threading.h"
#include "../sdk/framework/utils/intersect_impl.h"
#include "../sdk/framework/utils/intersect_box_impl.h"
#include "gametrace.h"

static constexpr int MAX_N = 256;

static int traceThreadBudget = 500;
static int cachedTraceThreadBudget = 5000;

static Mutex enumLock;
static CEntityListAlongRay enumerators[NUM_THREADS + 1];
static TraceCache cache[NUM_THREADS + 1];

void Tracing2::TraceRayListBSPOnly(size_t n, const Ray_t* rays, unsigned int mask, trace_t* traces)
{
	CTraceFilterWorldOnly worldFilter;

	cache[Threading::threadID + 1].traceCountTick += n;

	for (size_t i = 0; i < n; i++)
		engineTrace->TraceRay(rays[i], mask, &worldFilter, traces + i);
}

float wFraction[NUM_THREADS + 1][MAX_N];
float wFractionLeftSolid[NUM_THREADS + 1][MAX_N];
Ray_t entRayVec[NUM_THREADS + 1][MAX_N];
trace_t entTraceVec[NUM_THREADS + 1][MAX_N];
//Too huge to be allocated in bss memory
std::vector<CEntityListAlongRay> entEnumVec[NUM_THREADS + 1];
size_t newTraces[NUM_THREADS + 1][MAX_N];
float minFractions[NUM_THREADS + 1][MAX_N];

void Tracing2::TraceRayTargetOptimized(size_t n, trace_t* __restrict traces, Ray_t* __restrict rays, unsigned int mask, ITraceFilter* filter, int eID, Players* players)
{
	if (!n)
		return;

	CTraceFilterWorldOnly worldFilter;
	int threadIDX = Threading::threadID + 1;

	size_t tc = 0;

	for (size_t i = 0; i < n; i++) {
		float minFraction = rays[i].delta.Length() / CACHED_TRACE_LEN;

		if (cache[threadIDX].cachedTraceFindTick <= cachedTraceThreadBudget) {
			//Check both the local and the global trace caches for the closest ray
			const trace_t* globalEntry = cache[0].Find(rays[i]);
			const trace_t* localEntry = threadIDX ? cache[threadIDX].Find(rays[i]) : nullptr;
			cache[threadIDX].cachedTraceFindTick++;

			//We have got a cache entry, now just check if the closer one does not intersect the world
			if (globalEntry || localEntry) {
				//TODO: Distance check end point for the closer trace of the two
				if (!localEntry || (globalEntry && globalEntry->fraction > localEntry->fraction))
					traces[i] = *globalEntry;
				else
					traces[i] = *localEntry;

				//TODO: Perform autowall

				if (traces[i].fraction >= minFraction)
					GameTrace::CM_ClearTrace(traces + i);
				else
					traces[i].fraction /= minFraction;

				traces[i].endpos = rays[i].start + rays[i].delta * traces[i].fraction;

				cache[threadIDX].cachedTraceCountTick++;
				continue;
			}
		}

		//Only allow cached traces to be used when over budget
		if (cache[threadIDX].traceCountTick > traceThreadBudget)
			continue;

		cache[threadIDX].traceCountTick++;
		newTraces[threadIDX][tc++] = i;
		minFractions[threadIDX][i] = minFraction;

		rays[i].delta *= (1.f / minFraction);

		engineTrace->TraceRay(rays[i], mask, &worldFilter, traces + i);
		if (filter->GetTraceType() == TraceType::TRACE_ENTITIES_ONLY) {
			GameTrace::CM_ClearTrace(traces + i);
			traces[i].startpos = rays[i].start + rays[i].startOffset;
		    traces[i].endpos = (vec3_t)traces[i].startpos + rays[i].delta;
		}
	}

#ifdef DEBUG
	if (tc > 256)
		throw;
#endif

	bool filterStaticProps = false;

	trace_t* entTraces = entTraceVec[threadIDX];
	Ray_t* entRays = entRayVec[threadIDX];
	entEnumVec[threadIDX].resize(tc);
	CEntityListAlongRay* entEnums = entEnumVec[threadIDX].data();
	float* worldFraction = wFraction[threadIDX], *worldFractionLeftSolid = wFractionLeftSolid[threadIDX];

	//TODO: Make rw buffer accesses linear sequential
	for (size_t o = 0; o < tc; o++) {
		size_t i = newTraces[threadIDX][o];
		entRays[i] = GameTrace::ClipRayToWorldTrace(rays[i], traces + i, worldFraction + i, worldFractionLeftSolid + i);
	}

	for (size_t o = 0; o < tc; o++) {
		size_t i = newTraces[threadIDX][o];
		entEnums[i].count = 0;
		memset(entEnums[i].entityHandles, 0, sizeof(entEnums[i].entityHandles));
		spatialPartition->EnumerateElementsAlongRay(1 << 2, entRays[i], false, &entEnums[i]);
	}

	for (size_t u = 0; u < tc; u++) {
		size_t o = newTraces[threadIDX][u];
		trace_t* tr = traces + o;
		trace_t* trace = entTraces + o;
		Ray_t& entRay = entRays[o];
		CEntityListAlongRay& entEnum = entEnums[o];

		for (int i = 0; i < entEnum.count; i++) {

			//Early quit since we know that the only thing we care about here is direct LOS to the player
			if (tr->fraction * worldFraction[o] < 0.9f * minFractions[threadIDX][o])
				continue;

			IHandleEntity* handle = entEnum.entityHandles[i];
			ICollideable* col = nullptr;

			if (!handle)
				continue;

			bool staticProp = staticPropMgr->IsStaticProp(handle);

			col = GameTrace::GetCollideable(handle, staticProp);

			if (!col)
				col = ((C_BasePlayer*)handle)->GetCollideable();

			if ((!staticProp || filterStaticProps) && !filter->ShouldHitEntity(handle, mask))
				continue;

			GameTrace::ClipRayToCollideable(entRay, mask, col, trace, handle, staticProp);
			GameTrace::ClipTraceToTrace(*trace, tr);

			if (tr->allsolid)
				break;
		}
	}

	for (size_t o = 0; o < tc; o++) {
		size_t i = newTraces[threadIDX][o];
	    traces[i].fraction *= worldFraction[i];
		traces[i].fractionleftsolid *= worldFractionLeftSolid[i];

		cache[threadIDX].Push(traces[i]);

		if (traces[i].fraction >= minFractions[threadIDX][i]) {
			GameTrace::CM_ClearTrace(traces + i);
			traces[i].startpos = rays[i].start + rays[i].startOffset;
		    traces[i].endpos = (vec3_t)traces[i].startpos + rays[i].delta * minFractions[threadIDX][i];
		}
	}
}

trace_t awallTraceVec[NUM_THREADS + 1][MAX_N][AutoWall::MAX_INTERSECTS * 4];

void Tracing2::PenetrateRayTargetOptimized(size_t n, float* __restrict damageOutput, trace_t* __restrict traces, Ray_t* __restrict rays, unsigned int mask, ITraceFilter* filter, int eID, Players* players)
{
	if (!n)
		return;

	CTraceFilterWorldOnly worldFilter;
	int threadIDX = Threading::threadID + 1;

	size_t tc = 0;

	for (size_t i = 0; i < n; i++) {
		float minFraction = rays[i].delta.Length() / CACHED_TRACE_LEN;

		if (cache[threadIDX].cachedTraceFindTick <= cachedTraceThreadBudget) {
			//Check both the local and the global trace caches for the closest ray
			const trace_t* globalEntry = cache[0].Find(rays[i]);
			const trace_t* localEntry = threadIDX ? cache[threadIDX].Find(rays[i]) : nullptr;
			cache[threadIDX].cachedTraceFindTick++;

			//We have got a cache entry, now just check if the closer one does not intersect the world
			if (globalEntry || localEntry) {
				//TODO: Distance check end point for the closer trace of the two
				if (!localEntry || (globalEntry && globalEntry->fraction > localEntry->fraction))
					traces[i] = *globalEntry;
				else
					traces[i] = *localEntry;

				//TODO: Perform autowall

				if (traces[i].fraction >= minFraction)
					GameTrace::CM_ClearTrace(traces + i);
				else
					traces[i].fraction /= minFraction;

				traces[i].endpos = rays[i].start + rays[i].delta * traces[i].fraction;

				cache[threadIDX].cachedTraceCountTick++;
				continue;
			}
		}

		//Only allow cached traces to be used when over budget
		if (cache[threadIDX].traceCountTick > traceThreadBudget)
			continue;

		cache[threadIDX].traceCountTick++;
		newTraces[threadIDX][tc++] = i;
		minFractions[threadIDX][i] = minFraction;

		rays[i].delta *= (1.f / minFraction);

		engineTrace->TraceRay(rays[i], mask, &worldFilter, traces + i);
		if (filter->GetTraceType() == TraceType::TRACE_ENTITIES_ONLY) {
			GameTrace::CM_ClearTrace(traces + i);
			traces[i].startpos = rays[i].start + rays[i].startOffset;
		    traces[i].endpos = (vec3_t)traces[i].startpos + rays[i].delta;
		}
	}

#ifdef DEBUG
	if (tc > 256)
		throw;
#endif

	bool filterStaticProps = false;

	trace_t* entTraces = entTraceVec[threadIDX];
	Ray_t* entRays = entRayVec[threadIDX];
	entEnumVec[threadIDX].resize(tc);
	CEntityListAlongRay* entEnums = entEnumVec[threadIDX].data();
	float* worldFraction = wFraction[threadIDX], *worldFractionLeftSolid = wFractionLeftSolid[threadIDX];

	for (size_t o = 0; o < tc; o++) {
		size_t i = newTraces[threadIDX][o];
		entRays[i] = GameTrace::ClipRayToWorldTrace(rays[i], traces + i, worldFraction + i, worldFractionLeftSolid + i);
	}

	for (size_t o = 0; o < tc; o++) {
		size_t i = newTraces[threadIDX][o];
		entEnums[i].count = 0;
		memset(entEnums[i].entityHandles, 0, sizeof(entEnums[i].entityHandles));
		spatialPartition->EnumerateElementsAlongRay(1 << 2, entRays[i], false, &entEnums[i]);
	}

	for (size_t u = 0; u < tc; u++) {
		size_t o = newTraces[threadIDX][u];
		trace_t* tr = traces + o;
		trace_t* trace = entTraces + o;
		Ray_t& entRay = entRays[o];
		CEntityListAlongRay& entEnum = entEnums[o];

		for (int i = 0; i < entEnum.count; i++) {

			//Early quit since we know that the only thing we care about here is direct LOS to the player
			if (tr->fraction * worldFraction[o] < 0.9f * minFractions[threadIDX][o])
				continue;

			IHandleEntity* handle = entEnum.entityHandles[i];
			ICollideable* col = nullptr;

			if (!handle)
				continue;

			bool staticProp = staticPropMgr->IsStaticProp(handle);

			col = GameTrace::GetCollideable(handle, staticProp);

			if (!col)
				col = ((C_BasePlayer*)handle)->GetCollideable();

			if ((!staticProp || filterStaticProps) && !filter->ShouldHitEntity(handle, mask))
				continue;

			GameTrace::ClipRayToCollideable(entRay, mask, col, trace, handle, staticProp);
			GameTrace::ClipTraceToTrace(*trace, tr);

			if (tr->allsolid)
				break;
		}
	}

	for (size_t o = 0; o < tc; o++) {
		size_t i = newTraces[threadIDX][o];
	    traces[i].fraction *= worldFraction[i];
		traces[i].fractionleftsolid *= worldFractionLeftSolid[i];

		cache[threadIDX].Push(traces[i]);

		if (traces[i].fraction >= minFractions[threadIDX][i]) {
			GameTrace::CM_ClearTrace(traces + i);
			traces[i].startpos = rays[i].start + rays[i].startOffset;
		    traces[i].endpos = (vec3_t)traces[i].startpos + rays[i].delta * minFractions[threadIDX][i];
		}
	}
}

void Tracing2::GameTraceRay(const Ray_t& ray, unsigned int mask, ITraceFilter* filter, trace_t* tr)
{
    cache[Threading::threadID + 1].traceCountTick++;
	GameTrace::TraceRay(ray, mask, filter, tr, Threading::threadID);
}

void Tracing2::TracePart1(vec3_t eyePos, vec3_t point, trace_t* tr, C_BasePlayer* skient)
{
	Ray_t ray;
	CTraceFilterSkipPlayers filter;

	cache[Threading::threadID + 1].traceCountTick++;

	ray.Init(eyePos, point);

	Tracing2::GameTraceRay(ray, MASK_SHOT, &filter, tr);
}

float Tracing2::TracePart2(vec3_t eyePos, float weaponDamage, float weaponRangeModifier, Players* players, trace_t* tr, int eID)
{
	if (eID < 0 && tr->ent != FwBridge::localPlayer)
		return -1.f;

	//TODO: add target entity intersection test here in case we are not directly aiming at entity
	if (eID >= 0 && tr->ent && (void*)tr->ent != players->instance[eID])
		return -1.f;

	if (false && eID >= 0) {
		unsigned int flag = 0;

		for (size_t i = 0; i < NumOfSIMD(MAX_HITBOXES); i++)
			flag |= players->colliders[eID][i].Intersect(eyePos, tr->endpos);

		if (!flag)
			return -1.f;
	}

	int hbID = FwBridge::hitboxIDs[tr->hitbox];

	if (hbID < 0)
		return -1.f;

	float distance = ((vec3)eyePos - tr->endpos).Length();
	float damage = weaponDamage * powf(weaponRangeModifier, distance * 0.002f);

	//HACK HACK HACK this is very unsafe, we just assume there will be a player
	if (eID < 0)
		return damage * players->hitboxes[0].damageMul[hbID];
	return (int)(damage * players->hitboxes[eID].damageMul[hbID]);
}

int Tracing2::TracePlayers(vec3_t eyePos, float weaponDamage, float weaponRangeModifier, Players* players, vec3_t point, int eID, int depth, C_BasePlayer* skient)
{
	trace_t tr;
	TracePart1(eyePos, point, &tr, skient);
	return TracePart2(eyePos, weaponDamage, weaponRangeModifier, players, &tr, eID);
}

void Tracing2::ResetTraceCount()
{
	vec3_t pos = FwBridge::lpData.origin;
	//TODO: Add active weapon check to see if autowall cache invalidation is needed
	bool invalidate = (pos - cache[0].pos).LengthSqr<3>() > 1.f;

	cache[0].Reset(invalidate, nullptr);

	for (int i = 1; i < NUM_THREADS + 1; i++)
		cache[i].Reset(invalidate, cache);
}

int Tracing2::RetreiveTraceCount()
{
	int tc = 0;
	idx_t sz = 0;

	for (auto& i : cache) {
		tc += i.traceCountTick;
		tc += i.cachedTraceCountTick;
		sz += i.tree.size();
	}

    //cvar->ConsoleDPrintf(ST("CACHE SIZE: %u\n"), sz);

	return tc;
}

bool Tracing2::TraceBudgetEmpty()
{
	return cache[Threading::threadID + 1].traceCountTick >= traceThreadBudget;
}

float Tracing2::ScaleDamage(Players* players, int id, float in, float armorPenetration, int hitbox, HitGroups hitgroup)
{
	float armorSupport = false;

	C_BasePlayer* ent = (C_BasePlayer*)players->instance[id];

	float out = in;

	switch (hitgroup) {
	  case HitGroups::HITGROUP_HEAD:
		  armorSupport = ent->hasHelmet();
		  break;
	  case HitGroups::HITGROUP_GENERIC:
	  case HitGroups::HITGROUP_CHEST:
	  case HitGroups::HITGROUP_STOMACH:
	  case HitGroups::HITGROUP_LEFTARM:
	  case HitGroups::HITGROUP_RIGHTARM:
		  armorSupport = true;
		  break;
	  default:
		  break;
	}

	armorSupport = armorSupport && players->armor[id] > 0;

	out *= players->hitboxes[id].damageMul[hitbox];

	if (armorSupport) {
		float bonusValue = 1.f;
		float armorBonusRatio = 0.5f;
		float armorRatio = armorPenetration * 0.5f;

		if (ent->hasHeavyArmor()) {
			bonusValue = armorBonusRatio = 1.f / 3.f;
			armorRatio *= 0.5f;
		}

		if ((out - out * armorRatio) * (bonusValue * armorBonusRatio) > players->armor[id])
			out -= players->armor[id] / armorBonusRatio;
		else {
		    out *= armorRatio;

			if (ent->hasHeavyArmor())
			    out *= 0.85f;
		}
	}

	//cvar->ConsoleDPrintf("%f -> %f\n", in, out);

	return out;
}

int Tracing2::ClipTraceToPlayers(trace_t* tr, Players* players, uint64_t ignoreFlags)
{
	vec3_t start = tr->startpos;
	vec3_t end = tr->endpos;
	vec3_t dir = (end - start).Normalized();

	float len = (end - start).Length();
	float origLen = len;

	Players& curPlayers = FwBridge::playerTrack[0];

	int retPlayer = -1;

	//Test AABB colliders of all the players
	for (size_t i = 0; i < players->count; i++) {
		int pID = players->Resort(curPlayers, i);

		if (pID >= MAX_PLAYERS || ~curPlayers.flags[pID] & Flags::UPDATED || ignoreFlags & (1ull << pID))
			continue;

		AABBCollider aabb(players->origin[i] + players->boundsStart[i], players->origin[i] + players->boundsEnd[i]);

		bool ret = aabb.Intersect(start, end);

		if (ret) {

			studiohdr_t* hdr = FwBridge::cachedHDRs[players->unsortIDs[i]];
			mstudiohitboxset_t* set = hdr->GetHitboxSet(0);

			if (!set)
				continue;

			nvec3 iOut[NumOfSIMD(MAX_HITBOXES)];

			uint64_t flags = 0;

			for (size_t o = 0; o < NumOfSIMD(MAX_HITBOXES); o++)
				flags |= players->colliders[i][o].Intersect(start, end, iOut + o) << (SIMD_COUNT * o);

			float curBestDMGMul = 0;

			for (size_t o = 0; o < MAX_HITBOXES; o++)
				if (flags & (1ull << o)) {
					vec3_t iEnd = (vec3_t)iOut[o / MAX_HITBOXES].acc[o % SIMD_COUNT];
					float iLen = (iEnd - start).Length();

					float dmgMul = players->hitboxes[i].damageMul[o];

					//Arms and legs are able to pass through to head, other hitboxes are able to pass through to pelvis
					bool passingThrough = curBestDMGMul && ((curBestDMGMul < 1 && dmgMul >= 2.f) || (curBestDMGMul <= 1.f && dmgMul > curBestDMGMul && dmgMul < 2.f));
					bool passedThrough = curBestDMGMul && ((dmgMul < 1 && curBestDMGMul >= 2.f) || (dmgMul <= 1.f && curBestDMGMul > dmgMul && curBestDMGMul < 2.f));
					if (iLen < origLen && ((iLen < len && !passedThrough) || passingThrough)) {
						len = iLen;
						end = start + dir * len;

						mstudiobbox_t* box = set->GetHitbox(FwBridge::reHitboxIDs[o]);
					    mstudiobone_t* bone = hdr->GetBone(box->bone);

						tr->hitgroup = (HitGroups)box->group;
						tr->hitbox = FwBridge::reHitboxIDs[o];
						tr->contents = bone->contents | CONTENTS_HITBOX;
						tr->physicsbone = bone->physicsbone;
						tr->surface.name = "**studio**";
						tr->surface.flags = SURF_HITBOX;
						tr->surface.surfaceProps = bone->surfacepropidx;
						tr->ent = (IClientEntity*)players->instance[i];

						curBestDMGMul = fmaxf(curBestDMGMul, dmgMul);

						retPlayer = players->unsortIDs[i];
					}
				}
		}
	}

	float fracMul = len / origLen;

	tr->endpos = end;
	tr->fraction *= fracMul;

	return retPlayer;
}

template<size_t N>
void Tracing2::TracePlayersSIMD(vec3_t eyePos, float weaponDamage, float weaponRangeModifier, Players* players, vec3soa<float, N> point, int eID, int out[N], int depth, C_BasePlayer* skient)
{
	trace_t tr[N];
	for (size_t i = 0; i < N; i++)
		TracePart1(eyePos, (vec3_t)point.acc[i], tr + i, skient);
	for (size_t i = 0; i < N; i++)
		out[i] = TracePart2(eyePos, weaponDamage, weaponRangeModifier, players, tr + i, eID);
}

template void Tracing2::TracePlayersSIMD<MULTIPOINT_COUNT>(vec3_t eyePos, float weaponDamage, float weaponRangeModifier, Players* players, mvec3 point, int eID, int out[MULTIPOINT_COUNT], int depth, C_BasePlayer* skient);
