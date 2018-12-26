#ifndef GAMETRACE_H
#define GAMETRACE_H

#include "../sdk/source_csgo/sdk.h"

namespace GameTrace {
	extern IClientEntity* worldEnt;

	//Thread-safe rebuilds of engine tracing functions (almost 1 to 1 copy)
	//Basically good job, Valve, epic completely useless thread-unsafe global you got there
	void CM_ClearTrace(trace_t* tr);
	bool ClipTraceToTrace(const trace_t& __restrict cliptrace, trace_t* __restrict finalTrace);
	bool ShouldPerformCustomRayTest(const Ray_t& ray, ICollideable* collideable);
	bool ClipRayToCustom(const Ray_t& ray, unsigned int mask, ICollideable* collideable, trace_t* trace);
	bool ClipRayToBBox(const Ray_t &ray, unsigned int mask, ICollideable* entity, trace_t* trace, const matrix3x4_t* rootMoveParent);
	void SetTraceEntity(ICollideable* collideable, trace_t* trace, IHandleEntity* ent, bool staticProp);
	void ClipRayToCollideable(const Ray_t& ray, unsigned int mask, ICollideable* entity, trace_t* trace, IHandleEntity* ent, bool staticProp);
	ICollideable* GetCollideable(IHandleEntity* handle, bool staticProp);
	Ray_t ClipRayToWorldTrace(const Ray_t& ray, trace_t* tr, float* worldFraction, float* worldFractionLeftSolid);
	void TraceRay(const Ray_t& ray, unsigned int mask, ITraceFilter* filter, trace_t* tr, int threadID);
}

#endif
