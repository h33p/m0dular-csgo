#include "engine.h"
#include "../sdk/framework/math/mmath.h"
#include "../sdk/source_csgo/sdk.h"
#define SOURCE_DEFINITIONS
#include "source_features.h"

#include <algorithm>

C_BasePlayer* Engine::localPlayer = nullptr;
C_BaseCombatWeapon* Engine::activeWeapon = nullptr;
LocalPlayer Engine::lpData;

static ConVar* gameType = nullptr;
static ConVar* gameMode = nullptr;

bool Engine::IsEnemy(C_BasePlayer* ent)
{
	if (!gameType)
		gameType = cvar->FindVar("game_type");

	if (!gameMode)
		gameMode = cvar->FindVar("game_mode");

	// Danger zone
	if (gameType->GetInt() == 6 && !gameMode->GetInt()) {
		if (Engine::localPlayer->survivalTeamNum() == -1)
			return true;
		return Engine::localPlayer->survivalTeamNum() ^ ent->survivalTeamNum();
	}

	return ent->teamNum() ^ Engine::localPlayer->teamNum();
}

vec3_t Engine::PredictAimPunchAngle()
{
	return Engine::localPlayer->aimPunchAngle();
}

static ConVar* sensitivity = nullptr;
static ConVar* pitch = nullptr;
static ConVar* yaw = nullptr;
static ConVar* zoomSensitivityRatioMouse = nullptr;

vec2 Engine::GetMouseSensitivity()
{
	if (!sensitivity)
		sensitivity = cvar->FindVar("sensitivity");
	if (!pitch)
		pitch = cvar->FindVar("m_pitch");
	if (!yaw)
		yaw = cvar->FindVar("m_yaw");
	if (!zoomSensitivityRatioMouse)
		zoomSensitivityRatioMouse = cvar->FindVar("zoom_sensitivity_ratio_mouse");

	float sensVal = sensitivity ? sensitivity->GetFloat() : 1;
	float pitchVal = pitch ? pitch->GetFloat() : 1;
	float yawVal = yaw ? yaw->GetFloat() : 1;
	float zoomSensVal = zoomSensitivityRatioMouse ? zoomSensitivityRatioMouse->GetFloat() : 1;

	return vec2(yawVal * sensVal * zoomSensVal, pitchVal * sensVal * zoomSensVal);
}

bool noFog = false;

static void FrameUpdateOtherEnts()
{
	if (Engine::localPlayer)
		Engine::localPlayer->skybox3dFogEnable() = !noFog;

	int entCnt = entityList->GetMaxEntities();

	C_FogController* fogController = nullptr;

	// Here we skip past player entities
	for (int i = 63; i < entCnt; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)entityList->GetClientEntity(i);

		if (!ent)
			continue;

		ClientClass* clientClass = ent->GetClientClass();

		if (!clientClass)
			continue;

		switch (clientClass->classID) {
		  case ClassId_CCascadeLight:
			  break;
		  case ClassId_CFogController:
			  fogController = (C_FogController*)ent;
			  break;
		  default:
			  break;
		}
	}

	if (fogController)
		fogController->fogEnable() = !noFog;
}

//Players could have changed in this state, let's just loop the engine entity list
void Engine::FrameUpdate()
{
	FrameUpdateOtherEnts();
}

static ConVar* weapon_recoil_scale = nullptr;

static void UpdateFlags(int& flags, int& cflags, C_BasePlayer* ent);

void Engine::UpdateLocalPlayer()
{
	localPlayer = (C_BasePlayer*)entityList->GetClientEntity(engine->GetLocalPlayer());
	activeWeapon = nullptr;

	if (!localPlayer)
		return;

	activeWeapon = localPlayer->activeWeapon();
}

void Engine::UpdateLocalData(CUserCmd* cmd)
{
	UpdateLocalPlayer();

	if (activeWeapon) {
		lpData.weaponAmmo = activeWeapon->clip1();
	} else {
		lpData.weaponAmmo = 0;
	}

	lpData.velocity = localPlayer->velocity();
	lpData.origin = localPlayer->origin();
	lpData.time = globalVars->interval_per_tick * localPlayer->tickBase();

	float recoilScale = 1.f;

	if (!weapon_recoil_scale)
		weapon_recoil_scale = cvar->FindVar("weapon_recoil_scale");

	if (weapon_recoil_scale)
		recoilScale = weapon_recoil_scale->GetFloat();

	lpData.aimOffset = Engine::PredictAimPunchAngle() * recoilScale;

	int flags = localPlayer->flags();
	int cflags = 0;
	UpdateFlags(flags, cflags, localPlayer);
	lpData.flags = cflags;

	SourceEssentials::UpdateData(cmd, &lpData);
}

static void UpdateFlags(int& flags, int& cflags, C_BasePlayer* ent)
{
	cflags = Flags::EXISTS;
	if (flags & FL_ONGROUND)
		cflags |= Flags::ONGROUND;
	if (flags & FL_DUCKING)
		cflags |= Flags::DUCKING;
	if (Engine::localPlayer) {
		if (!Engine::IsEnemy(ent))
			cflags |= Flags::FRIENDLY;
	}
}
