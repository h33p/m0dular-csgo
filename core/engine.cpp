#include "engine.h"
#include "fw_bridge.h"
#include "../sdk/framework/math/mmath.h"
#include "../sdk/source_csgo/sdk.h"
#include <algorithm>

bool Engine::UpdatePlayer(C_BaseEntity* ent, matrix<3,4> matrix[128])
{
	*(int*)((uintptr_t)ent + x64x32(0xFEC, 0xA30)) = globalVars->framecount;
	*(int*)((uintptr_t)ent + x64x32(0xFE4, 0xA28)) = 0;
	*(unsigned long*)((uintptr_t)ent + x64x32(0x2C48, 0x2680)) = 0;

	ent->m_varMapping().m_nInterpolatedEntries = 0;
	if (!ent->SetupBones(matrix, MAXSTUDIOBONES, BONE_USED_BY_HITBOX, globalVars->curtime))
		return false;
	return true;
}

static ConVar* bigUdRate = nullptr;
static ConVar* minUdRate = nullptr;
static ConVar* maxUdRate = nullptr;
static ConVar* interpRatio = nullptr;
static ConVar* clInterp = nullptr;
static ConVar* minInterp = nullptr;
static ConVar* maxInterp = nullptr;

float Engine::LerpTime()
{
	if (!bigUdRate)
		bigUdRate = cvar->FindVar(("cl_updaterate"));
	if (!minUdRate)
		minUdRate = cvar->FindVar(("sv_minupdaterate"));
	if (!maxUdRate)
		maxUdRate = cvar->FindVar(("sv_maxupdaterate"));
	if (!interpRatio)
		interpRatio = cvar->FindVar(("cl_interp_ratio"));
	if (!clInterp)
		clInterp = cvar->FindVar(("cl_interp"));
	if (!minInterp)
		minInterp = cvar->FindVar(("sv_client_min_interp_ratio"));
	if (!maxInterp)
		maxInterp = cvar->FindVar(("sv_client_max_interp_ratio"));

	float updateRate = bigUdRate->GetFloat();

	if (minUdRate && maxUdRate)
		updateRate = std::clamp(updateRate, (float)(int)minUdRate->GetFloat(), (float)(int)maxUdRate->GetFloat());

	float ratio = interpRatio->GetFloat();

	float lerp = clInterp->GetFloat();

	if (minInterp && maxInterp && minInterp->GetFloat() != -1)
		ratio = std::clamp(ratio, minInterp->GetFloat(), maxInterp->GetFloat());
	else if (ratio == 0)
		ratio = 1.f;

	return std::max(lerp, ratio / updateRate);
}

float Engine::CalculateBacktrackTime()
{
	INetChannelInfo* nci = engine->GetNetChannelInfo();

	float correct = (nci ? nci->GetAvgLatency(FLOW_OUTGOING) + nci->GetAvgLatency(FLOW_INCOMING) : 0.f);

	float lerpTime = LerpTime();

	correct += lerpTime;
	correct = fmaxf(0.f, fminf(correct, 1.f));

    return globalVars->curtime * globalVars->interval_per_tick - 0.2f - correct;
}
