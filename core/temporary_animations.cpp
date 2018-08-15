#include "temporary_animations.h"
#include "fw_bridge.h"

TemporaryAnimations::TemporaryAnimations(C_BasePlayer* ent, float timeOffset, bool setTime)
{
	Init(ent, timeOffset, setTime);
}

TemporaryAnimations::TemporaryAnimations()
{
	init = false;
}

TemporaryAnimations::~TemporaryAnimations()
{
	if (!init)
		return;

	RestoreState();
	ent->angles() = angles;
	ent->eyeAngles() = eyeAngles;

	globalVars->curtime = curtime;
	globalVars->framecount = framecount;
	globalVars->frametime = frametime;
}

void TemporaryAnimations::Init(C_BasePlayer* ent, float timeOffset, bool setTime)
{
	init = true;
	this->ent = ent;
	angles = ent->angles();
	eyeAngles = ent->eyeAngles();
	curtime = globalVars->curtime;
	frametime = globalVars->frametime;
	framecount = globalVars->framecount;
	animState = *ent->animState();
	memcpy(layers, ent->animationLayers(), sizeof(layers));

	if (setTime)
		SetTime(timeOffset);
}

void TemporaryAnimations::RestoreState()
{
	*ent->animState() = animState;
	memcpy(ent->animationLayers(), layers, sizeof(layers));
}

void TemporaryAnimations::SetTime(float timeOffset)
{
	globalVars->curtime = ent->prevSimulationTime() + globalVars->interval_per_tick - timeOffset;
	globalVars->framecount = ent->animState()->frameCount + 1;
	globalVars->frametime = globalVars->interval_per_tick;
}
