#ifndef TRACING2_H
#define TRACING2_H

class C_BasePlayer;

namespace Tracing2
{
	int TracePlayers(vec3_t eyePos, float weaponDamage, float weaponRangeModifier, Players* players, vec3_t point, int eID, int depth, C_BasePlayer* skipEnt = nullptr);
	template<size_t N>
	void TracePlayersSIMD(vec3_t eyePos, float weaponDamage, float weaponRangeModifier, Players* players, vec3soa<float, N> point, int eID, int out[N], int depth, C_BasePlayer* skipEnt);
}

#endif
