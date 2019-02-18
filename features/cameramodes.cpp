#include "cameramodes.h"
#include "../core/fw_bridge.h"
#include "../core/engine.h"
#include "../core/settings.h"
#include "../sdk/features/gametrace.h"

static constexpr float THIRD_PERSON_DISTANCE = 150;

void CameraModes::OverrideView(CViewSetup* setup)
{
	//TODO: do this with the currently observed entity rather than local player

	C_BasePlayer* ent = FwBridge::localPlayer;

	if (!ent)
		return;

	if (Settings::thirdPerson) {
		vec3_t forward, right, up, origin;
		((vec3_t)setup->angles).GetVectors(forward, right, up, true);

		//TODO: Clean this up
#ifdef _WIN32
		Weapon_ShootPosition(ent, origin);
#else
		origin = Weapon_ShootPosition(ent);
#endif

		//TODO: Add option to skip all players
		CTraceFilterSkipPlayers filter;
		Ray_t ray;
		ray.Init(origin, forward * -THIRD_PERSON_DISTANCE, vec3_t(-19.f), vec3_t(19.f));
		trace_t tr;
		GameTrace::TraceRay(ray, MASK_SOLID, &filter, &tr, -1);

		tr.fraction = fmaxf(tr.fraction, 0.0f);
		setup->origin = origin - forward * THIRD_PERSON_DISTANCE * tr.fraction;
		input->cameraOffset[2] = tr.fraction * THIRD_PERSON_DISTANCE;
	}
#ifdef TESTING_FEATURES
	else if (Settings::headCam) {
		studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());

		if (!hdr)
			return;

		mstudiohitboxset_t* set = hdr->GetHitboxSet(0);

		if (!set)
			return;

		mstudiobbox_t* headBox = set->GetHitbox(Hitboxes::HITBOX_HEAD);
		matrix3x4_t headMatrix = Engine::GetDirectBone(ent, &hdr, headBox->bone);
		vec3_t mid = headMatrix.Vector3Transform((headBox->bbmax + headBox->bbmin) * 0.5f);
		setup->origin = mid;
		input->cameraOffset[2] = 0;
	}
#endif
}
