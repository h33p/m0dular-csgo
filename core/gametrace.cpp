#include "gametrace.h"
#include "fw_bridge.h"

IClientEntity* GameTrace::worldEnt = nullptr;

void GameTrace::CM_ClearTrace(trace_t* tr)
{
	csurface_t emptyStruct = { (const char*)"**empty**", (int)0, (int)0 };

	memset((void*)tr, 0, sizeof(trace_t));
	tr->fraction = 1.f;
	tr->fractionleftsolid = 0;
	tr->surface = emptyStruct;
}

bool GameTrace::ClipTraceToTrace(const trace_t& __restrict cliptrace, trace_t* __restrict finalTrace)
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

bool GameTrace::ShouldPerformCustomRayTest(const Ray_t& ray, ICollideable* collideable)
{
	// No model? The entity's got its own collision detector maybe
	// Does the entity force box or ray tests to go through its code?
	return ((collideable->GetSolid() == SolidType_t::SOLID_CUSTOM) ||
			(ray.isRay && (collideable->GetSolidFlags() & SolidFlags_t::FSOLID_CUSTOMRAYTEST)) ||
			(!ray.isRay && (collideable->GetSolidFlags() & SolidFlags_t::FSOLID_CUSTOMBOXTEST)));
}

bool GameTrace::ClipRayToCustom(const Ray_t& ray, unsigned int mask, ICollideable* collideable, trace_t* trace)
{
	if (collideable->TestCollision(ray, mask, *trace))
		return true;

	return false;
}

bool GameTrace::ClipRayToBBox(const Ray_t &ray, unsigned int mask, ICollideable* entity, trace_t* trace, const matrix3x4_t* rootMoveParent)
{
	if (entity->GetSolid() != SolidType_t::SOLID_BBOX)
		return false;

	alignas(16) vec3_t vecAbsMins, vecAbsMaxs;
	alignas(16) vec3_t vecInvDelta;
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

		//It is best to rebuilt this function, since it is not all that difficult, but in the meantime we will have to use this assembly hack
		IntersectRayWithBox(rayL, vecInvDelta, vecAbsMins, vecAbsMaxs, trace);
#ifdef _WIN32
		_asm add esp, 0xC;
#endif

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
#ifdef _WIN32
	_asm add esp, 0xC;
#endif
	return true;
}

void GameTrace::SetTraceEntity(ICollideable* collideable, trace_t* trace, IHandleEntity* ent, bool staticProp)
{
	if (!staticProp) {
		trace->ent = (IClientEntity*)ent;
	} else {
		trace->ent = worldEnt;
		trace->hitbox = 0; //TODO: GetStaticPropIndex
	}
}

#ifdef __APPLE__
bool ClipRayToBSP(IEngineTrace* tracer, const Ray_t& ray, unsigned int mask, ICollideable* entity, trace_t* trace)
{
	int modelIndex = entity->GetCollisionModelIndex();
	cmodel_t* model = CM_InlineModelNumber(modelIndex - 1);

	TransformedBoxTrace(ray, model->headnode, mask, entity->GetCollisionOrigin(), entity->GetCollisionAngles(), trace);
	return true;
}

bool ClipRayToOBB(IEngineTrace* tracer, const Ray_t& ray, unsigned int mask, ICollideable* entity, trace_t* trace)
{
	if (entity->GetSolid() != SolidType_t::SOLID_OBB)
		return false;

	IntersectRayWithOBB(ray, entity->GetCollisionOrigin(), entity->GetCollisionAngles(), entity->OBBMins(), entity->OBBMaxs(), trace, 0.03125); //DIST_EPSILON
	return true;
}
#endif

//Hell, this can be optimized and rebuilt in a more data-oriented fashion. Even valve left their concerns. TODO: Do just that
void GameTrace::ClipRayToCollideable(const Ray_t& ray, unsigned int mask, ICollideable* entity, trace_t* trace, IHandleEntity* ent, bool staticProp)
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

	const matrix3x4_t* rootMoveParent;

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
		ClipRayToBBox(ray, mask, entity, trace, rootMoveParent);

	if (isStudioModel && !tracedHitboxes && trace->DidHit() && (!customPerformed || trace->surface.surfaceProps == 0)) {
		trace->contents = studioHdr->contents;
		// use the default surface properties
		trace->surface.name = "**studio**";
		trace->surface.flags = 0;
		trace->surface.surfaceProps = studioHdr->surfaceProp;
	}

	if (!trace->ent && trace->DidHit())
		SetTraceEntity(entity, trace, ent, staticProp);
}


ICollideable* GameTrace::GetCollideable(IHandleEntity* handle, bool staticProp)
{
	if (staticProp)
		return (ICollideable*)staticPropMgrClient->GetStaticProp(handle);

	return ((C_BaseEntity*)handle)->GetCollideable();
}

Ray_t GameTrace::ClipRayToWorldTrace(const Ray_t& ray, trace_t* tr, float* worldFraction, float* worldFractionLeftSolid)
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

void GameTrace::TraceRay(const Ray_t& ray, unsigned int mask, ITraceFilter* filter, trace_t* tr, int threadID)
{
	if (threadID == -1) {
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

	Ray_t entRay = GameTrace::ClipRayToWorldTrace(ray, tr, &worldFraction, &worldFractionLeftSolid);

	memset(entEnum.entityHandles, 0, sizeof(entEnum.entityHandles));

	spatialPartition->EnumerateElementsAlongRay(1 << 2, entRay, false, &entEnum);

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
