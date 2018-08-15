#ifndef TEMPORARY_ANIMATIONS_H
#define TEMPORARY_ANIMATIONS_H

#include "../sdk/framework/math/mmath.h"
#include "../sdk/source_csgo/sdk.h"

class TemporaryAnimations
{
  public:
	TemporaryAnimations(C_BasePlayer* ent, float timeOffset = 0, bool setTime = false);
	TemporaryAnimations();
	~TemporaryAnimations();
	void Init(C_BasePlayer* ent, float timeOffset = 0, bool setTime = false);
	void RestoreState();
	void SetTime(float timeOffset = 0);
  private:
	C_BasePlayer* ent;
	vec3_t angles;
	vec3_t eyeAngles;
	CCSGOPlayerAnimState animState;
	AnimationLayer layers[13];

	float curtime;
	float frametime;
	int framecount;
	bool init;
};

#endif
