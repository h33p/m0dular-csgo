#ifndef IMPACTS_H
#define IMPACTS_H

#include "../sdk/framework/math/mmath.h"

struct CEffectData;
class IGameEvent;

struct BulletData
{
	vec3_t pos;
	vec3_t relPos;
	vec3_t relStart;
	int attacker;
	int hitEnt;
	int hitbox;
	float addTime;
	int backTick;
	char onGround;

	bool cleared;
	float processed;
};

namespace Impacts
{
	void Tick();
	void ImpactEvent(IGameEvent* data, unsigned int name);
	void HandleImpact(const CEffectData& effectData);
}

#endif
