#ifndef ENGINE_H
#define ENGINE_H

#include "../sdk/framework/math/mmath.h"

struct Players;
class C_BasePlayer;

namespace Engine
{
	bool UpdatePlayer(C_BasePlayer* ent, matrix<3,4> out[128]);
	float LerpTime();
	float CalculateBacktrackTime();
	void StartLagCompensation();
	void EndLagCompensation();
	void StartAnimationFix(Players* players, Players* prevPlayers);
	void Shutdown();
	void FrameUpdate();
	bool IsEnemy(C_BasePlayer* ent);
}

#endif
