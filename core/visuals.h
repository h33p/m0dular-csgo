#ifndef VISUALS_H
#define VISUALS_H

#include "../sdk/framework/math/mmath.h"

namespace Visuals
{
	extern bool shouldDraw;
#ifdef PT_VISUALS
	void Draw();
#endif
	void PassColliders(vec3soa<float, 16> start, vec3soa<float, 16> end);
	void PassStart(vec3_t start, vec3_t end);
	void PassBest(int o, int i);
	void SetShotVectors(vec3_t serverStart, vec3_t serverEnd, vec3_t idealStart, vec3_t idealEnd);
	void SetAwallBoxes(vec3_t* boxes, int count);
}

#endif
