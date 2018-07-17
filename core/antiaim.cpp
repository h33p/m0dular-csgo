#include "antiaim.h"
#include "fw_bridge.h"
#include "engine.h"
#include "temporary_animations.h"
#include "tracing.h"
#include "visuals.h"

static void CalculateBases();

static float viewAngle = 0.f;
static float atTargetAngle = 0.f;
static float atTargetAverageAngle = 0.f;
static float freeStandAngle = 0.f;
static bool safeAngles[FREESTAND_ANGLES];
static int lastFreeStandID = 0;

static bool applyAngles = false;

constexpr float FREESTAND_THRESHOLD = 10.f;

void Antiaim::Run(CUserCmd* cmd, FakelagState state)
{
	if (cmd->buttons & (IN_ATTACK | IN_ATTACK2 | IN_USE))
		return;

	LocalPlayer& lp = FwBridge::lpData;
	lp.angles.x = 89;
	CalculateBases();
	lp.angles.y = freeStandAngle + 180.f;
	applyAngles = true;
}

float Antiaim::CalculateFreestanding(int id, bool outAngles[FREESTAND_ANGLES])
{
	Players& players = FwBridge::playerTrack.GetLastItem(0);
	int count = players.count;
	vec3_t origin;
	int friendlyXOR = 0;
	vec3* angles = nullptr;
	C_BasePlayer* ent = nullptr;

	//Determine which angles we will modify
	if (id < 0) {
		ent = FwBridge::localPlayer;
		origin = FwBridge::lpData.origin;
		angles = &ent->localAngles();
	} else {
		ent = (C_BasePlayer*)players.instance[id];
		origin = players.origin[id];
		friendlyXOR = Flags::FRIENDLY ^ (players.flags[id] & Flags::FRIENDLY);
		angles = &ent->eyeAngles();
	}

	//Find the closest enemy player
	vec3_t closestPlayer = {{{0, 0, 0}}};
	float closestDistance = 10000000000000000.f;
	float weaponDamage = 0.f;
	float weaponRangeModifier = 0.f;
	C_BasePlayer* closestEnt = nullptr;

	for (int i = 0; i < count; i++) {
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		C_BaseCombatWeapon* weapon = ent->activeWeapon();
		if (!weapon)
			continue;
		CCSWeaponInfo* weaponInfo = GetWeaponInfo(weaponDatabase, weapon->itemDefinitionIndex());
		if (!weaponInfo || weaponInfo->iWeaponType() == WEAPONTYPE_KNIFE || weaponInfo->iWeaponType() >= WEAPONTYPE_C4)
			continue;
		float dist = (FwBridge::lpData.eyePos - players.eyePos[i]).LengthSqr();
		if (~(players.flags[i] ^ friendlyXOR) & Flags::FRIENDLY && dist < closestDistance) {
			closestDistance = dist;
			closestPlayer = players.eyePos[i];
			weaponDamage = weaponInfo->iDamage();
			weaponRangeModifier = weaponInfo->flRangeModifier();
			closestEnt = ent;
		}
	}

	if (!closestEnt)
		return 0.f;

	cvar->ConsoleDPrintf("CLOSEST\n");
	float bestAngle = (closestPlayer - origin).GetAngles(true)[1];

	//Get the index of the head bone
	studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());
	if (!hdr)
	    return bestAngle;

	mstudiohitboxset_t* set = hdr->GetHitboxSet(0);
	if (!set)
	    return bestAngle;

	mstudiobbox_t* hitbox = set->GetHitbox(Hitboxes::HITBOX_HEAD);
	if (!hitbox)
		return bestAngle;

	int boneID = hitbox->bone;

	matrix3x4_t matrix[128];
	TemporaryAnimations anims(ent, 1, true);
	vec3 anglesBackup = *angles;
	float gfyOffset = (ent->eyeAngles()[1] - ent->animState()->goalFeetYaw);

	float minDamage = FREESTAND_THRESHOLD;

	//The main loop, step by a predefined step and run aimbot from the closest player to the head hitbox
	for (int i = 0; i < FREESTAND_ANGLES; i++) {
		float ang = i * ANGLE_STEP;
		(*angles)[1] = NormalizeFloat(ang, -180.f, 180.f);
		ent->animState()->goalFeetYaw = NormalizeFloat(ang - gfyOffset, 0, 360);
		ent->UpdateClientSideAnimation();
		ent->angles()[0] = 0.f;
		ent->angles()[1] = ang;
		ent->angles()[2] = 0.f;
		SetAbsAngles(ent, ent->angles());
		anims.RestoreState();
		Engine::UpdatePlayer(ent, matrix);

		vec3_t headPos;
		headPos.x = matrix[boneID][0][3];
		headPos.y = matrix[boneID][1][3];
		headPos.z = matrix[boneID][2][3];

		int out[MULTIPOINT_COUNT];
		mvec3 mpVec = players.hitboxes[0].mpOffset[0] + players.hitboxes[0].mpDir[0] * players.hitboxes[0].radius[0] * 0.97f;
		mpVec = matrix[boneID].VecSoaTransform(mpVec);
		Tracing2::TracePlayersSIMD<MULTIPOINT_COUNT>(closestPlayer, weaponDamage, weaponRangeModifier, &players, mpVec, id, out, 2, closestEnt);

		int maxDMG = 0;

		for (size_t o = 0; o < MULTIPOINT_COUNT; o++)
		    maxDMG += out[o];

		float damage = maxDMG / MULTIPOINT_COUNT;

		if (damage < minDamage) {
			minDamage = damage;
			bestAngle = NormalizeFloat(ang + 180.f, 0.f, 360.f);
		}

		if (outAngles)
			outAngles[i] = damage < FREESTAND_THRESHOLD;
	}

	*angles = anglesBackup;
	SetAbsAngles(ent, ent->angles());
	Engine::UpdatePlayer(ent, matrix);
	return bestAngle;
}

static void CalculateBases()
{
	viewAngle = FwBridge::lpData.angles.y;

	Players& players = FwBridge::playerTrack.GetLastItem(0);
	int count = players.count;

	vec3_t averagePlayer = {{{0, 0, 0}}};
	int cnt = 0;

	for (int i = 0; i < count; i++) {
		if (~players.flags[i] & Flags::FRIENDLY) {
			cnt++;
			averagePlayer += players.eyePos[i];
		}
	}

	if (!cnt) {
		atTargetAngle = viewAngle;
		atTargetAverageAngle = viewAngle;
		freeStandAngle = viewAngle;
		return;
	}

	averagePlayer *= (1.f / cnt);
	atTargetAverageAngle = (averagePlayer - FwBridge::lpData.eyePos).GetAngles(true)[1];

	vec3_t closestPlayer = {{{0, 0, 0}}};
	float closestDistance = 10000000000000000.f;

	for (int i = 0; i < count; i++) {
		float dist = (FwBridge::lpData.eyePos - players.eyePos[i]).LengthSqr();
		if (~players.flags[i] & Flags::FRIENDLY && dist < closestDistance) {
			closestDistance = dist;
			closestPlayer = players.origin[i];
		}
	}

	atTargetAngle = (closestPlayer - FwBridge::lpData.eyePos).GetAngles(true)[1];

	float* poseParameter = &FwBridge::localPlayer->poseParameter();
	float poseParams[24];
	memcpy(poseParams, poseParameter, sizeof(poseParams));

	char val = *((char*)FwBridge::localPlayer + 0x42BD);
	*((char*)FwBridge::localPlayer + 0x42BD) = 1;
	{
		vec3_t vPitch = FwBridge::localPlayer->localAngles();
		TemporaryAnimations anims(FwBridge::localPlayer, 1.f, true);
		FwBridge::localPlayer->localAngles()[0] = FwBridge::lpData.angles.x;
		freeStandAngle = Antiaim::CalculateFreestanding(-1, safeAngles);
		anims.RestoreState();
		FwBridge::localPlayer->localAngles() = vPitch;
		FwBridge::localPlayer->UpdateClientSideAnimation();
		anims.RestoreState();
	}
	*((char*)FwBridge::localPlayer + 0x42BD) = val;
	SetAbsAngles(FwBridge::localPlayer, FwBridge::localPlayer->angles());
	Engine::UpdatePlayer(FwBridge::localPlayer, nullptr);

	if (safeAngles[lastFreeStandID])
		freeStandAngle = lastFreeStandID * ANGLE_STEP;
	lastFreeStandID = (int)(freeStandAngle / ANGLE_STEP + 0.5f);
}
