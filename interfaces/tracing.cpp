#include "../sdk/framework/interfaces/tracing.h"
#include "../core/fw_bridge.h"

int Tracing::TracePlayers(LocalPlayer* localPlayer, Players* players, vec3_t point, bool skipLocal)
{
	trace_t tr;
	Ray_t ray;
	CTraceFilter filter;

	ray.Init(localPlayer->eyePos, point);
	if (skipLocal)
		filter.pSkip = FwBridge::localPlayer;
	engineTrace->TraceRay(ray, MASK_SHOT, &filter, &tr);

	float distance = ((vec3)localPlayer->eyePos - tr.endpos).Length();

	return (int)distance;
}
