#ifndef LAGCOMPENSATION_H
#define LAGCOMPENSATION_H

#include "../sdk/framework/players.h"
#include "../sdk/framework/utils/history_list.h"

struct Circle
{
	vec3_t center;
	float radius;
	float invRadius;
	float direction;

	Circle() : radius(1.f), invRadius(1.f), direction(1.f)
	{
		center = 0.f;
	}

	Circle(vec3_t cent, float rad, float dir)
	{
		center = cent;
		radius = rad;
		direction = dir;
		invRadius = 1.f / radius;
	}

	//Doesn't care about z coordinate
	Circle(vec3_t a, vec3_t b, vec3_t c)
	{
		float yDelta_a = b.y - a.y;
		float xDelta_a = b.x - a.x;
		float yDelta_b = c.y - b.y;
		float xDelta_b = c.x - b.x;
		center = 0.f;

		float aSlope = yDelta_a / xDelta_a;
		float bSlope = yDelta_b / xDelta_b;
		center.x = (aSlope*bSlope*(a.y - c.y) + bSlope*(a.x + b.x)
			- aSlope*(b.x + c.x)) / (2 * (bSlope - aSlope));
		center.y = -1 * (center.x - (a.x + b.x) / 2) / aSlope + (a.y + b.y) / 2;

		radius = sqrtf((b.x - center.x) * (b.x - center.x) + (b.y - center.y) * (b.y - center.y));

		float vecMulA = a.y - center.y;
		float vecMulB = b.y - center.y;

		float vecMulA2 = a.x - center.x;
		float vecMulB2 = b.x - center.x;

		float angA1 = acosf(vecMulA / (radius * radius)) * RAD2DEG;
		float angA2 = acosf(vecMulA2 / (radius * radius)) * RAD2DEG;

		if (angA2 > 90)
			angA1 = 360.f - angA1;

		float angB1 = acosf(vecMulB / (radius * radius)) * RAD2DEG;
		float angB2 = acosf(vecMulB2 / (radius * radius)) * RAD2DEG;

		if (angB2 > 90)
			angB1 = 360.f - angB1;

		float angDiff = fmodf(angA1 - angB1 + 540, 360.f) - 180.f;

		direction = (angDiff > 0) ? -1 : 1;

		invRadius = 1.f / radius;
	}
};

namespace LagCompensation
{
	extern int quality;
	extern HistoryList<Players, BACKTRACK_TICKS>* futureTrack;
	void PreRun();
	void Run();
}

#endif
