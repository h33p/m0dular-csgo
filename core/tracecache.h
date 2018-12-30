#ifndef TRACECACHE_H
#define TRACECACHE_H

#include "awall.h"
#include "settings.h"
#include "../sdk/framework/utils/kd_tree.h"
#include "../sdk/framework/utils/freelistallocator.h"
#include "../sdk/framework/utils/allocwraps.h"

//Windows CSGO struggles already with memory
static constexpr size_t TRACE_CACHE_SIZE = x64x32(100000, 30000);
static constexpr size_t CAN_TRACE_PASS_SCALE = 10;

struct traceang_t
{
	//Further traces are used in autowall
	trace_t traces[AutoWall::MAX_INTERSECTS * 2 + 2];
	float damages[AutoWall::MAX_INTERSECTS * 2 + 2];
	bool permaCache[AutoWall::MAX_INTERSECTS * 2 + 2];
	int traceCount;
	bool awalled;
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

struct TraceCache
{

	int traceCountTick, cachedTraceCountTick, cachedTraceFindTick, traceCallCount;
	vec3_t pos;
	float eyeHeight;
	static constexpr uintptr_t nullBase = 0;
	KDTree<traceang_t, 2, free_list_allocator<TreeNode_t<traceang_t>, nullBase, false, TRACE_CACHE_SIZE>> tree;
	bool printed = false;

	TraceCache() : traceCountTick(0), cachedTraceCountTick(0), pos(0), eyeHeight(0), tree()
	{

	}

	bool CanTraceImpl(int count, int mul)
	{
		traceCallCount++;
		int curMul = mul;
		size_t passScale = 1;
		for (size_t i = 0; i < 4; i++) {
			if (count < Settings::traceBudget * curMul && !(traceCallCount % passScale))
				return true;
			curMul += mul;
			passScale *= CAN_TRACE_PASS_SCALE;
		}
		return false;
	}

	bool CanTraceCached()
	{
		return CanTraceImpl(cachedTraceCountTick, 10);
	}

	bool CanTrace()
	{
		return CanTraceImpl(traceCountTick, 1);
	}

	void Reset(bool invalidate, TraceCache* mergeTo)
	{
		traceCountTick = 0;
		cachedTraceCountTick = 0;
		cachedTraceFindTick = 0;
		traceCallCount = 0;

		eyeHeight = FwBridge::lpData.eyePos[2] - FwBridge::lpData.origin[2];

		//A temporary workaround until we find the core issue of cache size going out of hand
		if (tree.size() > TRACE_CACHE_SIZE - 6000)
			invalidate = true;

		if (invalidate) {
			pos = FwBridge::lpData.origin;
			tree.Clear();
			return;
		}

		if (!mergeTo)
			return;
	}

	traceang_t* Find(const Ray_t& ray)
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
			return &ref->value;

		return nullptr;
	}

	void Push(const trace_t& trace)
	{
		traceang_t entry;

		entry.traces[0] = trace;
		entry.awalled = false;
		vec3_t angles = (trace.endpos - trace.startpos).GetAngles(true);
		entry.ang[0] = angles[0];
		entry.ang[1] = angles[1];
		entry.dist = CACHED_TRACE_LEN;
		entry.eyeHeight = eyeHeight;

		if (tree.size() < TRACE_CACHE_SIZE - 5000)
			tree.Insert(entry);
	}

	void PushAwall(const trace_t* traces, float* damages, bool* permaCache, size_t traceCount)
	{
		traceang_t entry;

		assert(traceCount <= AutoWall::MAX_INTERSECTS * 2 + 2);

		memcpy(entry.traces, traces, sizeof(trace_t) * traceCount);
		memcpy(entry.damages, damages, sizeof(float) * traceCount);
		memcpy(entry.permaCache, permaCache, sizeof(bool) * traceCount);
		entry.awalled = true;
		vec3_t angles = (traces[0].endpos - traces[0].startpos).GetAngles(true);
		entry.ang[0] = angles[0];
		entry.ang[1] = angles[1];
		entry.dist = CACHED_TRACE_LEN;
		entry.eyeHeight = eyeHeight;

		if (tree.size() < TRACE_CACHE_SIZE - 5000)
			tree.Insert(entry);
	}
};


#endif
