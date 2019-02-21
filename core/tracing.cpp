#include "tracing.h"
#include "hooks.h"
#include "../sdk/features/gametrace.h"
#include "tracecache.h"
#include "../features/awall.h"
#include "../sdk/framework/utils/threading.h"
#include "../sdk/framework/utils/intersect_impl.h"
#include "../sdk/framework/utils/intersect_box_impl.h"
#include "mtr_scoped.h"

static constexpr int MAX_N = 64;

static Mutex enumLock;
static TraceCache cache[NUM_THREADS + 1];


void Tracing2::TraceRayListBSPOnly(size_t n, const Ray_t* rays, unsigned int mask, trace_t* traces)
{
	CTraceFilterWorldOnly worldFilter;

	cache[Threading::threadID + 1].traceCountTick += n;

	for (size_t i = 0; i < n; i++)
		engineTrace->TraceRay(rays[i], mask, &worldFilter, traces + i);
}

CEntityListAlongRay entEnumVec[NUM_THREADS + 1][MAX_N];

void Tracing2::TraceRayTargetOptimized(size_t n, trace_t* __restrict traces, Ray_t* __restrict rays, unsigned int mask, ITraceFilter* filter, int eID, Players* players)
{
	if (!n)
		return;

    MTR_SCOPED_TRACE("Tracing2", "TraceRayTargetOptimized");

	CTraceFilterWorldOnly worldFilter;
	int threadIDX = Threading::threadID + 1;

	for (size_t off = 0; off < n; off += MAX_N) {
		size_t tc = 0;

		float minFractions[MAX_N];
		size_t newTraces[MAX_N];

		for (size_t i = 0; i < n - off; i++) {
			float minFraction = rays[off + i].delta.Length() / CACHED_TRACE_LEN;

			if (cache[threadIDX].CanTraceCached()) {
				//Check both the local and the global trace caches for the closest ray
				const traceang_t* globalEntry = cache[0].Find(rays[off + i]);
				const traceang_t* localEntry = threadIDX ? cache[threadIDX].Find(rays[off + i]) : nullptr;
				cache[threadIDX].cachedTraceFindTick++;

				//We have got a cache entry, now just check if the closer one does not intersect the world
				if (globalEntry || localEntry) {
					//TODO: Distance check end point for the closer trace of the two
					if (!localEntry || (globalEntry && globalEntry->traces[0].fraction > localEntry->traces[0].fraction))
						traces[off + i] = globalEntry->traces[0];
					else
						traces[off + i] = localEntry->traces[0];

					if (traces[off + i].fraction >= minFraction)
						GameTrace::CM_ClearTrace(traces + off + i);
					else
						traces[off + i].fraction /= minFraction;

					traces[off + i].endpos = rays[off + i].start + rays[off + i].delta * traces[off + i].fraction;
					cache[threadIDX].traceCountTick++;
					continue;
				}
			}

			//Only allow cached traces to be used when over budget
			if (!cache[threadIDX].CanTrace())
				continue;

			cache[threadIDX].traceCountTick++;
			newTraces[tc] = off + i;
			minFractions[tc++] = minFraction;

			rays[off + i].delta *= (1.f / minFraction);

			engineTrace->TraceRay(rays[i], mask, &worldFilter, traces + off + i);
			if (filter->GetTraceType() == TraceType::TRACE_ENTITIES_ONLY) {
				GameTrace::CM_ClearTrace(traces + i);
				traces[off + i].startpos = rays[off + i].start + rays[off + i].startOffset;
				traces[off + i].endpos = (vec3_t)traces[off + i].startpos + rays[off + i].delta;
			}
		}

#ifdef DEBUG
		if (tc > MAX_N)
			throw;
#endif

		bool filterStaticProps = false;

		trace_t entTraces[MAX_N];
		Ray_t entRays[MAX_N];
		CEntityListAlongRay* entEnums = entEnumVec[threadIDX];
		float worldFraction[MAX_N], worldFractionLeftSolid[MAX_N];

		//TODO: Make rw buffer accesses linear sequential
		for (size_t o = 0; o < tc; o++) {
			size_t i = newTraces[o];
			entRays[o] = GameTrace::ClipRayToWorldTrace(rays[i], traces + i, worldFraction + o, worldFractionLeftSolid + o);
		}

		for (size_t o = 0; o < tc; o++) {
			entEnums[o].count = 0;
			memset(entEnums[o].entityHandles, 0, sizeof(entEnums[o].entityHandles));
			spatialPartition->EnumerateElementsAlongRay(1 << 2, entRays[o], false, &entEnums[o]);
		}

		for (size_t u = 0; u < tc; u++) {
			size_t o = newTraces[u];
			trace_t* tr = traces + o;
			trace_t* trace = entTraces + u;
			Ray_t& entRay = entRays[u];
			CEntityListAlongRay& entEnum = entEnums[u];

			for (int i = 0; i < entEnum.count; i++) {

				//Early quit since we know that the only thing we care about here is direct LOS to the player
				if (tr->fraction * worldFraction[u] < 0.9f * minFractions[u])
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
			size_t i = newTraces[o];
			traces[i].fraction *= worldFraction[o];
			traces[i].fractionleftsolid *= worldFractionLeftSolid[o];

			cache[threadIDX].Push(traces[i]);

			if (traces[i].fraction >= minFractions[o]) {
				GameTrace::CM_ClearTrace(traces + i);
				traces[i].startpos = rays[i].start + rays[i].startOffset;
				traces[i].endpos = (vec3_t)traces[i].startpos + rays[i].delta * minFractions[o];
			}
		}
	}
}

void Tracing2::PenetrateRayTargetOptimized(size_t n, float* __restrict damageOutput, Ray_t* __restrict rays, int eID, Players* players, LocalPlayer* localPlayer)
{
	if (!n)
		return;

    MTR_SCOPED_TRACE("Tracing2", "PenetrateRayTargetOptimized");

	int threadIDX = Threading::threadID + 1;

	for (size_t i = 0; i < n; i++) {

		damageOutput[i] = 0.f;

		if (cache[threadIDX].CanTraceCached()) {
			//Check both the local and the global trace caches for the closest ray
			traceang_t* globalEntry = cache[0].Find(rays[i]);
			traceang_t* localEntry = threadIDX ? cache[threadIDX].Find(rays[i]) : nullptr;
			cache[threadIDX].cachedTraceFindTick++;

			//We have got a cache entry, now just check if the closer one does not intersect the world
			if (globalEntry || localEntry) {
				if (localEntry && localEntry->awalled) {
					damageOutput[i] = AutoWall::FireBulletPlayers(localPlayer->eyePos, rays[i].delta.Normalized(), localPlayer->weaponRange, localPlayer->weaponRangeModifier, localPlayer->weaponDamage, localPlayer->weaponPenetration, localPlayer->weaponArmorPenetration, &localEntry->traceCount, localEntry->permaCache, localEntry->traces, localEntry->damages, players);
					cache[threadIDX].cachedTraceFindTick += localEntry->traceCount;
					cache[threadIDX].traceCountTick++;
					continue;
				} else if (globalEntry && globalEntry->awalled) {
					damageOutput[i] = AutoWall::FireBulletPlayers(localPlayer->eyePos, rays[i].delta.Normalized(), localPlayer->weaponRange, localPlayer->weaponRangeModifier, localPlayer->weaponDamage, localPlayer->weaponPenetration, localPlayer->weaponArmorPenetration, &globalEntry->traceCount, globalEntry->permaCache, globalEntry->traces, globalEntry->damages, players);
					cache[threadIDX].cachedTraceFindTick += globalEntry->traceCount;
					cache[threadIDX].traceCountTick++;
					continue;
				}
			}
		}

		//Only allow cached traces to be used when over budget
		if (!cache[threadIDX].CanTrace())
			continue;

		int cacheSize = 0;
		bool permaCache[AutoWall::MAX_INTERSECTS * 2 + 2];
		trace_t outTraces[AutoWall::MAX_INTERSECTS * 2 + 2];
		float outDamages[AutoWall::MAX_INTERSECTS * 2 + 2];
		AutoWall::FireBulletWorld(localPlayer->eyePos, rays[i].delta.Normalized(), localPlayer->weaponRange, localPlayer->weaponRangeModifier, localPlayer->weaponDamage, localPlayer->weaponPenetration, &cacheSize, permaCache, outTraces, outDamages);

		cache[threadIDX].PushAwall(outTraces, outDamages, permaCache, cacheSize);

		damageOutput[i] = AutoWall::FireBulletPlayers(localPlayer->eyePos, rays[i].delta.Normalized(), localPlayer->weaponRange, localPlayer->weaponRangeModifier, localPlayer->weaponDamage, localPlayer->weaponPenetration, localPlayer->weaponArmorPenetration, &cacheSize, permaCache, outTraces, outDamages, players);

	}
}

void Tracing2::GameTraceRay(const Ray_t& ray, unsigned int mask, ITraceFilter* filter, trace_t* tr)
{
    cache[Threading::threadID + 1].traceCountTick++;
	GameTrace::TraceRay(ray, mask, filter, tr, Threading::threadID);
}

int Tracing2::GetPointContents(const vec3& pos, int mask, IHandleEntity** ent)
{
    cache[Threading::threadID + 1].traceCountTick++;
	return engineTrace->GetPointContents(pos, mask, ent);
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
	if (tr->ent && (void*)tr->ent != FwBridge::GetPlayer(*players, eID))
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

	//TODO: Use ScaleDamage
	//HACK HACK HACK this is very unsafe, we just assume there will be a player
	if (eID < 0)
		return damage * players->hitboxes[0].damageMul[hbID];
	return (int)(damage * players->hitboxes[eID].damageMul[hbID]);
}

int Tracing2::TracePlayers(vec3_t eyePos, float weaponDamage, float weaponRangeModifier, Players* players, vec3_t point, int eID, int depth, C_BasePlayer* skipent)
{
	trace_t tr;
	TracePart1(eyePos, point, &tr, skipent);
	return TracePart2(eyePos, weaponDamage, weaponRangeModifier, players, &tr, eID);
}

void Tracing2::ResetTraceCount()
{
	MTR_SCOPED_TRACE("Tracing2", "ResetTraceCount");

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
	return cache[Threading::threadID + 1].traceCountTick >= Settings::traceBudget;
}

float Tracing2::ScaleDamage(Players* players, int id, float in, float armorPenetration, int hitbox, HitGroups hitgroup)
{
	float armorSupport = false;

	C_BasePlayer* ent = FwBridge::GetPlayer(*players, id);

	//TODO: Log this occurance
	if (!ent)
		return in;

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
	for (int i = 0; i < players->count; i++) {
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
						tr->ent = (IClientEntity*)FwBridge::GetPlayer(*players, i);

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
