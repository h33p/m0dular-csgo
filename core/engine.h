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
	matrix3x4_t GetDirectBone(C_BasePlayer* ent, studiohdr_t** hdr, size_t boneID);
	bool CopyBones(C_BasePlayer* ent, matrix3x4_t* matrix, int size);
	void StartAnimationFix(Players* players, Players* prevPlayers);
	void Shutdown();
	void FrameUpdate();
	bool IsEnemy(C_BasePlayer* ent);
	vec3_t PredictAimPunchAngle();
	vec2 GetMouseSensitivity();
}

#endif
