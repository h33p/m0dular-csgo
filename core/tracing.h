#ifndef TRACING2_H
#define TRACING2_H

#include "../sdk/framework/math/mmath.h"

class C_BasePlayer;
struct Ray_t;
class IEntityEnumerator;
class CGameTrace;
typedef CGameTrace trace_t;
class ITraceFilter;
struct Players;

namespace Tracing2
{
	int TracePlayers(vec3_t eyePos, float weaponDamage, float weaponRangeModifier, Players* players, vec3_t point, int eID, int depth, C_BasePlayer* skipEnt = nullptr);
	template<size_t N>
	void TracePlayersSIMD(vec3_t eyePos, float weaponDamage, float weaponRangeModifier, Players* players, vec3soa<float, N> point, int eID, int out[N], int depth, C_BasePlayer* skipEnt);

	void GameTraceRay(const Ray_t& ray, unsigned int mask, ITraceFilter* traceFilter, trace_t* trace);
	void TracePart1(vec3_t eyePos, vec3_t point, trace_t* tr, C_BasePlayer* skipEnt);
	float TracePart2(vec3_t eyePos, float weaponDamage, float weaponRangeModifier, Players* players, trace_t* tr, int eID);

	void TraceRayListBSPOnly(size_t n, const Ray_t* rays, unsigned int mask, trace_t* traces);
	void TraceRayTargetOptimized(size_t n, trace_t* __restrict traces, Ray_t* __restrict rays, unsigned int mask, ITraceFilter* filter, int eID, Players* players);
	void ResetTraceCount();
	int RetreiveTraceCount();
}

#endif
