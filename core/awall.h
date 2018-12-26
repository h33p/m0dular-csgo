#ifndef AWALL_H
#define AWALL_H

#include "../sdk/source_csgo/sdk.h"

namespace AutoWall
{
	static constexpr int MAX_INTERSECTS = 4;

	bool TraceToExitWorld(const trace_t& __restrict inTrace, trace_t* __restrict outTrace, vec3_t startPos, vec3_t dir, bool* inBreakable, bool* outBreakable);
	bool HandleBulletPenetrationWorld(const trace_t& inTrace, vec3_t dir, bool lastHit, float penetrationPower, bool sv_penetration_type, float ff_damage_reduction_bullets, float ff_damage_bullet_penetration, bool* inBreakable, bool* outBreakable, float* curDamage, trace_t* outTrace);
	void FireBulletWorld(vec3_t start, vec3_t dir, float weaponRange, float weaponRangeModifier, float weaponDamage, float weaponPenetration, int* curOutID, bool* permaCache, trace_t* outTraces, float* outDamages);

	void FireBulletPlayers(vec3_t start, vec3_t dir, float weaponRange, float weaponRangeModifier, float weaponDamage, float weaponPenetration, int cacheSize, const bool* permaCache, const trace_t* inTraces, const float* inDamages);
}

#endif
