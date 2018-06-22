#ifndef ENGINE_H
#define ENGINE_H

#include "../sdk/framework/math/mmath.h"

class C_BaseEntity;

namespace Engine
{
	bool UpdatePlayer(C_BaseEntity* ent, matrix<3,4> out[128]);
	float LerpTime();
	float CalculateBacktrackTime();
}

#endif
