#include "cameramodes.h"
#include "../core/fw_bridge.h"
#include "../core/engine.h"
#include "../core/settings.h"
#include "../sdk/features/gametrace.h"

void CameraModes::OverrideView(CViewSetup* setup)
{
	//TODO: do this with the currently observed entity rather than local player

	C_BasePlayer* ent = FwBridge::localPlayer;

	if (!ent)
		return;

	studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());

	if (Settings::thirdPerson) {
		vec3_t forward, right, up;
		((vec3_t)setup->angles).GetVectors(forward, right, up, true);

		//TODO: Add option to skip all players
		CTraceFilterSkipEntity filter(ent);
		Ray_t ray;
		ray.Init(setup->origin, forward * -150);
		trace_t tr;
		GameTrace::TraceRay(ray, MASK_SHOT_HULL, &filter, &tr, -1);
		setup->origin -= forward * 150 * tr.fraction;
	}
#ifdef TESTING_FEATURES
	else if (Settings::headCam) {
		if (!hdr)
			return;

		mstudiohitboxset_t* set = hdr->GetHitboxSet(0);

		if (!set)
			return;

		mstudiobbox_t* headBox = set->GetHitbox(Hitboxes::HITBOX_HEAD);
		matrix3x4_t headMatrix = Engine::GetDirectBone(ent, &hdr, headBox->bone);
		vec3_t mid = headMatrix.Vector3Transform((headBox->bbmax + headBox->bbmin) * 0.5f);
		setup->origin = mid;
	}
#endif
}
