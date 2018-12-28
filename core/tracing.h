#ifndef TRACING2_H
#define TRACING2_H

#include "awall.h"
#include "fw_bridge.h"
#include "../sdk/source_csgo/sdk.h"
#include "../sdk/framework/math/mmath.h"
#include "../sdk/framework/utils/kd_tree.h"
#include "../sdk/framework/utils/freelistallocator.h"
#include "../sdk/framework/utils/allocwraps.h"

class C_BasePlayer;
struct Ray_t;
class IEntityEnumerator;
class CGameTrace;
typedef CGameTrace trace_t;
class ITraceFilter;
struct Players;

static constexpr float CACHED_TRACE_LEN = 8192.f;
static constexpr float CACHE_ACCURACY = 1.f;

struct TraceCache
{
	struct traceang_t
	{
		//Further traces are used in autowall
		trace_t traces[AutoWall::MAX_INTERSECTS + 2];
		int traceCount;
		vec2 ang;
		float eyeHeight;
		float dist;

		//Returns the distance between traces at the end of the current trace. Returns FLT_MAX if the right hand side trace is not suitable for comparision
		//NOTE: make sure the cached trace goes past the target (till the weapon range) so we never have false positives and all cached traces are usable all the time
		inline float operator-(const traceang_t& o) const
		{

			//The height check makes sure that while standing still and ducking/unducking multiple times does not require us to invalidate the trace cache
			//dist check should never be false when testing against cache entry (only query might have smaller distance to cover)
			if (fabsf(eyeHeight - o.eyeHeight) > 1.f || o.dist < dist)
				return std::numeric_limits<float>::max();

			//We do not need such high angular precision when dealing with traces really close to the source, however in large distances it is extremely important
			float angDiff = (ang - o.ang).Length<2>();
			float angTan = tanf(angDiff * DEG2RAD);
			float angLen = dist * angTan;

			if (o.dist < sqrtf(angLen * angLen + dist * dist))
				return std::numeric_limits<float>::max();

			return angLen;
		}

		inline bool operator==(const traceang_t& o) const
		{
			return fabsf(*this - o) < CACHE_ACCURACY;
		}

		inline bool operator!=(const traceang_t& o) const
		{
			return !(*this == o);
		}

		inline float operator[](int idx) const
		{
			return ang[idx];
		}
	};

	int traceCountTick, cachedTraceCountTick, cachedTraceFindTick;
	vec3_t pos;
	float eyeHeight;
	static constexpr uintptr_t nullBase = 0;
	KDTree<traceang_t, 2, free_list_allocator<TreeNode_t<traceang_t>, nullBase, false, 200000>> tree;

	TraceCache() : traceCountTick(0), cachedTraceCountTick(0), pos(0), eyeHeight(0), tree()
	{

	}

	void Reset(bool invalidate, TraceCache* mergeTo)
	{
		traceCountTick = 0;
		cachedTraceCountTick = 0;
		cachedTraceFindTick = 0;

		eyeHeight = FwBridge::lpData.eyePos[2] - FwBridge::lpData.origin[2];

		//A temporary workaround until we find the core issue of cache size going out of hand
		if (tree.size() > 100000)
			invalidate = true;

		if (invalidate) {
			pos = FwBridge::lpData.origin;
			tree.Clear();
			return;
		}

		if (!mergeTo)
			return;
	}

	const trace_t* Find(const Ray_t& ray)
	{
	    traceang_t entry;

		vec3_t angles = ray.delta.GetAngles(true);
		entry.ang[0] = angles[0];
		entry.ang[1] = angles[1];
		entry.dist = ray.delta.Length();
		entry.eyeHeight = eyeHeight;

		auto ref = tree.Find(entry);

		//TODO: Handle this for autowall
		if (ref)
			return &ref->value.traces[0];

		return nullptr;
	}

	void Push(const trace_t& trace)
	{
		//TODO: Handle this for autowall
		traceang_t entry;

		entry.traces[0] = trace;
		vec3_t angles = (trace.endpos - trace.startpos).GetAngles(true);
		entry.ang[0] = angles[0];
		entry.ang[1] = angles[1];
		entry.dist = CACHED_TRACE_LEN;
		entry.eyeHeight = eyeHeight;

		tree.Insert(entry);
	}
};

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
	void PenetrateRayTargetOptimized(size_t n, float* __restrict damageOutput, trace_t* __restrict traces, Ray_t* __restrict rays, unsigned int mask, ITraceFilter* filter, int eID, Players* players);
	void ResetTraceCount();
	int RetreiveTraceCount();
	bool TraceBudgetEmpty();

	float ScaleDamage(Players* players, int id, float in, float armorPenetration, int hitbox, HitGroups hitgroup);

	int ClipTraceToPlayers(trace_t* tr, Players* players, uint64_t ignoreFlags);
}

#endif
