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
}

#endif
