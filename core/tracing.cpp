#include "tracing.h"
#include "fw_bridge.h"
#include "hooks.h"
#include "../sdk/source_csgo/sdk.h"
#include "../sdk/framework/utils/threading.h"
#include "../sdk/framework/utils/intersect_impl.h"

std::atomic_int Tracing2::traceCounter = 0;

static Mutex enumLock;
static CEntityListAlongRay enumerators[NUM_THREADS + 1];

static IClientEntity* worldEnt = nullptr;

//Good job, Valve, epic thread-unsafe globals you got threre
thread_local static const matrix3x4_t* rootMoveParent = nullptr;

static void CM_ClearTrace(trace_t* tr)
{
    csurface_t emptyStruct = { (const char*)"**empty**", (int)0, (int)0 };

	memset(tr, 0, sizeof(trace_t));
	tr->fraction = 1.f;
	tr->fractionleftsolid = 0;
	tr->surface = emptyStruct;
}

static bool ClipTraceToTrace(const trace_t& __restrict cliptrace, trace_t* __restrict finalTrace)
{
	if (cliptrace.allsolid || cliptrace.startsolid || (cliptrace.fraction < finalTrace->fraction)) {
		if (finalTrace->startsolid) {
			float fractionLeftSolid = finalTrace->fractionleftsolid;
			vec3_t sPos = finalTrace->startpos;

			*finalTrace = cliptrace;
			finalTrace->startsolid = true;

			if (fractionLeftSolid > cliptrace.fractionleftsolid) {
				finalTrace->fractionleftsolid = fractionLeftSolid;
				finalTrace->startpos = sPos;
			}
		} else
			*finalTrace = cliptrace;

		return true;
	}

	if (cliptrace.startsolid) {
		finalTrace->startsolid = true;

		if (cliptrace.fractionleftsolid > finalTrace->fractionleftsolid) {
			finalTrace->fractionleftsolid = cliptrace.fractionleftsolid;
			finalTrace->startpos = cliptrace.startpos;
		}
	}

	return false;
}

static inline bool ShouldPerformCustomRayTest(const Ray_t& ray, ICollideable* collideable)
{
	// No model? The entity's got its own collision detector maybe
	// Does the entity force box or ray tests to go through its code?
	return ((collideable->GetSolid() == SolidType_t::SOLID_CUSTOM) ||
			(ray.isRay && (collideable->GetSolidFlags() & SolidFlags_t::FSOLID_CUSTOMRAYTEST)) ||
			(!ray.isRay && (collideable->GetSolidFlags() & SolidFlags_t::FSOLID_CUSTOMBOXTEST)));
}

static bool ClipRayToCustom(const Ray_t& ray, unsigned int mask, ICollideable* collideable, trace_t* trace)
{
	if (collideable->TestCollision(ray, mask, *trace))
		return true;

	return false;
}

static bool ClipRayToBBox(const Ray_t &ray, unsigned int mask, ICollideable* entity, trace_t* trace)
{
	if (entity->GetSolid() != SolidType_t::SOLID_BBOX)
		return false;

	vec3_t vecAbsMins, vecAbsMaxs;
	vec3_t vecInvDelta;
	// NOTE: If rootMoveParent is set, then the boxes should be rotated into the root parent's space
	if (!ray.isRay && rootMoveParent) {
		Ray_t rayL;

		rayL.extents = ray.extents;

		rayL.delta = rootMoveParent->Vector3IRotate(ray.delta);
		rayL.startOffset = vec3(0);
		rayL.start = rootMoveParent->Vector3ITransform(ray.start);

		vecInvDelta = rayL.InvDelta();
		vec3 localEntityOrigin;
		localEntityOrigin = rootMoveParent->Vector3ITransform(entity->GetCollisionOrigin());
		rayL.isRay = ray.isRay;
		rayL.isSwept = ray.isSwept;

		vecAbsMins = localEntityOrigin + entity->OBBMins();
		vecAbsMaxs = localEntityOrigin + entity->OBBMaxs();

		IntersectRayWithBox(rayL, vecInvDelta, vecAbsMins, vecAbsMaxs, trace);

		if (trace->DidHit()) {
		    trace->plane.normal = rootMoveParent->Vector3Rotate(trace->plane.normal);
			trace->startpos = ray.start + ray.startOffset;

			if (trace->fraction == 1)
				trace->endpos = trace->startpos + ray.delta;
			else
				trace->endpos = trace->startpos + (vec3)(ray.delta * trace->fraction);
			trace->plane.dist = trace->endpos.Dot(trace->plane.normal);
			if ( trace->fractionleftsolid < 1 )
				trace->startpos += ray.delta * trace->fractionleftsolid;
		} else
			trace->startpos = ray.start + ray.startOffset;

		return true;
	}

	vecInvDelta = ray.InvDelta();
	vecAbsMins = entity->GetCollisionOrigin() + entity->OBBMins();
	vecAbsMaxs = entity->GetCollisionOrigin() + entity->OBBMaxs();
	IntersectRayWithBox(ray, vecInvDelta, vecAbsMins, vecAbsMaxs, trace);
	return true;
}

static void SetTraceEntity(ICollideable* collideable, trace_t* trace, IHandleEntity* ent, bool staticProp)
{
	if (!staticProp) {
		trace->ent = (IClientEntity*)ent;
	} else {
		trace->ent = worldEnt;
		trace->hitbox = 0; //TODO: GetStaticPropIndex
	}
}

#ifdef __APPLE__
static bool ClipRayToBSP(IEngineTrace* tracer, const Ray_t& ray, unsigned int mask, ICollideable* entity, trace_t* trace)
{
	int modelIndex = entity->GetCollisionModelIndex();
	cmodel_t* model = CM_InlineModelNumber(modelIndex - 1);

	TransformedBoxTrace(ray, model->headnode, mask, entity->GetCollisionOrigin(), entity->GetCollisionAngles(), trace);
	return true;
}

static bool ClipRayToOBB(IEngineTrace* tracer, const Ray_t& ray, unsigned int mask, ICollideable* entity, trace_t* trace)
{
	if (entity->GetSolid() != SolidType_t::SOLID_OBB)
		return false;

	IntersectRayWithOBB(ray, entity->GetCollisionOrigin(), entity->GetCollisionAngles(), entity->OBBMins(), entity->OBBMaxs(), trace, 0.03125); //DIST_EPSILON
	return true;
}
#endif

static void ClipRayToCollideable(const Ray_t& ray, unsigned int mask, ICollideable* entity, trace_t* trace, IHandleEntity* ent, bool staticProp)
{
	CM_ClearTrace(trace);

	trace->startpos = ray.start + ray.startOffset;
	trace->endpos = trace->startpos + ray.delta;

	const model_t* model = entity->GetCollisionModel();
	bool isStudioModel = false;
	studiohdr_t* studioHdr = NULL;

	if (model && model->type == 3) { //mod_studio = 3
		isStudioModel = true;
		studioHdr = (studiohdr_t*)modelLoader->GetExtraData((model_t*)model);
		// Cull if the collision mask isn't set + we're not testing hitboxes.
		if (((mask & CONTENTS_HITBOX) == 0))
			if ((mask & studioHdr->contents) == 0)
				return;
	}

	if (entity->GetSolidFlags() & SolidFlags_t::FSOLID_ROOT_PARENT_ALIGNED)
		rootMoveParent = entity->GetRootParentToWorldTransform();

	bool traced = false;
	bool customPerformed = false;

	if (ShouldPerformCustomRayTest(ray, entity)) {
		ClipRayToCustom(ray, mask, entity, trace);
		traced = true;
		customPerformed = true;
	} else //TODO: Possibly rebuild this and cache data
		traced = ClipRayToVPhysics(engineTrace, ray, mask, entity, studioHdr, trace);

	// FIXME: Why aren't we using solid type to check what kind of collisions to test against?!?!
	if (!traced && model && model->type == 1) //mod_brush = 1
		traced = ClipRayToBSP(engineTrace, ray, mask, entity, trace);

	if (!traced)
		traced = ClipRayToOBB(engineTrace, ray, mask, entity, trace);

	// Hitboxes..
	bool tracedHitboxes = false;
	if (isStudioModel && (mask & CONTENTS_HITBOX)) {
		// Until hitboxes are no longer implemented as custom raytests,
		// don't bother to do the work twice
		if (!customPerformed) {
			//We do not even want to test hitboxes as we use custom hitbox tester
			//traced = ClipRayToHitboxes( ray, mask, entity, trace );
			if (traced) {
				// Hitboxes will set the surface properties
				tracedHitboxes = true;
			}
		}
	}

	if (!traced)
		ClipRayToBBox( ray, mask, entity, trace );

	if (isStudioModel && !tracedHitboxes && trace->DidHit() && (!customPerformed || trace->surface.surfaceProps == 0)) {
		trace->contents = studioHdr->contents;
		// use the default surface properties
		trace->surface.name = ST("**studio**");
		trace->surface.flags = 0;
		trace->surface.surfaceProps = studioHdr->surfaceProp;
	}

	if (!trace->ent && trace->DidHit())
		SetTraceEntity(entity, trace, ent, staticProp);
}


static ICollideable* GetCollideable(IHandleEntity* handle, bool staticProp)
{
	if (staticProp)
		return (ICollideable*)staticPropMgrClient->GetStaticProp(handle);

	return ((C_BaseEntity*)handle)->GetCollideable();
}

static Ray_t ClipRayToWorldTrace(const Ray_t& ray, trace_t* tr, float* worldFraction, float* worldFractionLeftSolid)
{
	Ray_t ret = ray;

	*worldFraction = tr->fraction;
	*worldFractionLeftSolid = *worldFraction;

	if (tr->fraction == 0) {
		ret.delta = vec3_t(0);
		*worldFractionLeftSolid = tr->fractionleftsolid;
		tr->fractionleftsolid = 1.f;
		tr->fraction = 1.f;
	} else {
		vec3_t end = ret.start + ret.delta * tr->fraction;
		ret.delta = end - ret.start;
		tr->fractionleftsolid /= tr->fraction;
		tr->fraction = 1.f;
	}

	return ret;
}

static void EnumerateElements(unsigned int mask, const Ray_t& entRay, bool coarseTest, IEntityEnumerator* entEnum)
{
	//enumLock.lock();
	spatialPartition->EnumerateElementsAlongRay(mask, entRay, coarseTest, entEnum);
	//enumLock.unlock();
}

void Tracing2::TraceRayListBSPOnly(size_t n, const Ray_t* rays, unsigned int mask, trace_t* traces)
{
	CTraceFilterWorldOnly worldFilter;

	for (size_t i = 0; i < n; i++)
		engineTrace->TraceRay(rays[i], mask, &worldFilter, traces + i);
}

std::vector<float> wFraction[NUM_THREADS + 1], wFractionLeftSolid[NUM_THREADS + 1];
std::vector<Ray_t> entRayVec[NUM_THREADS + 1];
std::vector<trace_t> entTraceVec[NUM_THREADS + 1];
std::vector<CEntityListAlongRay> entEnumVec[NUM_THREADS + 1];

void Tracing2::ClipWorldTracesToWorldEntitiesTargetOptimized(size_t n, trace_t* __restrict traces, Ray_t* __restrict rays, unsigned int mask, ITraceFilter* filter, int eID, Players* players)
{

	if (!n)
		return;

	for (size_t i = 0; i < n; i++) {
		Tracing2::traceCounter++;
		if (filter->GetTraceType() == TraceType::TRACE_ENTITIES_ONLY) {
			CM_ClearTrace(traces + i);
			traces[i].startpos = rays[i].start + rays[i].startOffset;
		    traces[i].endpos = (vec3_t)traces[i].startpos + rays[i].delta;
		}
	}

	bool filterStaticProps = false;

	int threadIDX = Threading::threadID + 1;

	wFraction[threadIDX].resize(n);
	wFractionLeftSolid[threadIDX].resize(n);
	entRayVec[threadIDX].resize(n);
	entTraceVec[threadIDX].resize(n);
	entEnumVec[threadIDX].resize(n);

	trace_t* entTraces = entTraceVec[threadIDX].data();
	Ray_t* entRays = entRayVec[threadIDX].data();
	CEntityListAlongRay* entEnums = entEnumVec[threadIDX].data();
	float* worldFraction = wFraction[threadIDX].data(), *worldFractionLeftSolid = wFractionLeftSolid[threadIDX].data();

	for (size_t i = 0; i < n; i++)
	    entRays[i] = ClipRayToWorldTrace(rays[i], traces + i, worldFraction + i, worldFractionLeftSolid + i);

	for (int i = 0; i < n; i++) {
		entEnums[i].count = 0;
		memset(entEnums[i].entityHandles, 0, sizeof(entEnums[i].entityHandles));
		EnumerateElements(1 << 2, entRays[i], false, &entEnums[i]);
	}

	for (size_t o = 0; o < n; o++) {
		trace_t* tr = traces + o;
		trace_t* trace = entTraces + o;
		Ray_t& entRay = entRays[o];
		CEntityListAlongRay& entEnum = entEnums[o];

		for (int i = 0; i < entEnum.count; i++) {

			//Early quit since we know that the only thing we care about here is direct LOS to the player
			if (tr->fraction * worldFraction[o] < 0.9f)
				continue;

			IHandleEntity* handle = entEnum.entityHandles[i];
			ICollideable* col = nullptr;

			if (!handle)
				continue;

			bool staticProp = staticPropMgr->IsStaticProp(handle);

			col = GetCollideable(handle, staticProp);

			if (!col)
				col = ((C_BasePlayer*)handle)->GetCollideable();

			if ((!staticProp || filterStaticProps) && !filter->ShouldHitEntity(handle, mask))
				continue;

			ClipRayToCollideable(entRay, mask, col, trace, handle, staticProp);
			ClipTraceToTrace(*trace, tr);

			if (tr->allsolid)
				break;
		}
	}

	for (size_t i = 0; i < n; i++) {
	    traces[i].fraction *= worldFraction[i];
		traces[i].fractionleftsolid *= worldFractionLeftSolid[i];
	}
}

void Tracing2::GameTraceRay(const Ray_t& ray, unsigned int mask, ITraceFilter* filter, trace_t* tr)
{
	Tracing2::traceCounter++;

	if (Threading::threadID == -1) {
		engineTrace->TraceRay(ray, mask, filter, tr);
		return;
	}

	CM_ClearTrace(tr);

	if (filter->GetTraceType() != TraceType::TRACE_ENTITIES_ONLY) {
		CTraceFilterWorldOnly worldFilter;
		engineTrace->TraceRay(ray, mask, &worldFilter, tr);
	} else {
		tr->startpos = ray.start + ray.startOffset;
		tr->endpos = tr->startpos + ray.delta;
	}

	CEntityListAlongRay entEnum;

	trace_t trace;
	bool filterStaticProps = false;

	float worldFraction, worldFractionLeftSolid;

	Ray_t entRay = ClipRayToWorldTrace(ray, tr, &worldFraction, &worldFractionLeftSolid);

	memset(entEnum.entityHandles, 0, sizeof(entEnum.entityHandles));

	EnumerateElements(1 << 2, entRay, false, &entEnum);

	for (int i = 0; i < entEnum.count; i++) {
		IHandleEntity* handle = entEnum.entityHandles[i];
		ICollideable* col = nullptr;

		if (!handle)
			continue;

		bool staticProp = staticPropMgr->IsStaticProp(handle);

	    col = GetCollideable(handle, staticProp);

		if (!col)
			col = ((C_BasePlayer*)handle)->GetCollideable();

		if ((!staticProp || filterStaticProps) && !filter->ShouldHitEntity(handle, MASK_SHOT))
			continue;

		ClipRayToCollideable(entRay, mask, col, &trace, handle, staticProp);
		ClipTraceToTrace(trace, tr);

		if (tr->allsolid)
			break;
	}

	tr->fraction *= worldFraction;
	tr->fractionleftsolid *= worldFractionLeftSolid;
}

void Tracing2::TracePart1(vec3_t eyePos, vec3_t point, trace_t* tr, C_BasePlayer* skient)
{
	Ray_t ray;
	CTraceFilterSkipPlayers filter;

	Tracing2::traceCounter++;

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

	if (eID >= 0) {
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
