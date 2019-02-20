#include "spread.h"
#include "../core/fw_bridge.h"
#include "../core/mtr_scoped.h"
#include "../sdk/framework/utils/threading.h"
#include "../sdk/framework/utils/intersect_impl.h"

static float randomFl1[256];
static float randomFlPi1[256];
static float randomFl2[256];
static float randomFlPi2[256];

static float randomFlCos1[256];
static float randomFlSin1[256];

static float randomFlCos2[256];
static float randomFlSin2[256];

static bool randomPopulated = false;

constexpr int HITCHANCE_JOBS = 4;
constexpr int HITCHANCE_PER_JOB = 256 / HITCHANCE_JOBS;

struct HitChanceInput
{
	int start;
	int jobID;
};

static int tempOutput[HITCHANCE_JOBS];

static Players* hcPlayers;
static int targetEnt;
static vec3_t targetVec;
static vec3_t forward;
static vec3_t up;
static vec3_t right;
static float inaccuracyVal;
static float spreadVal;
static float range;
static vec3_t startPos;
CapsuleColliderSOA<SIMD_COUNT>* hcHitboxes;

static void PopulateRandomFloat();
static void RunHitChance(HitChanceInput* inp);

bool Spread::HitChance(Players* players, int targetEnt, vec3_t targetVec, int boneID, int chance)
{
	MTR_SCOPED_TRACE("Spread", "HitChance");

	if (!randomPopulated)
		PopulateRandomFloat();

	FwBridge::activeWeapon->UpdateAccuracyPenalty();

	hcPlayers = players;
	::targetEnt = targetEnt;
	::targetVec = targetVec;

	range = FwBridge::lpData.weaponRange;
	spreadVal = 0;//FwBridge::activeWeapon->GetSpread();
	inaccuracyVal = FwBridge::activeWeapon->GetInaccuracy();
	vec3_t dir = targetVec - FwBridge::lpData.eyePos;
	startPos = FwBridge::lpData.eyePos;
	dir.Normalize().ToAngles().GetVectors(forward, up, right);

	hcHitboxes = players->colliders[targetEnt];

	for (int i = 0; i < HITCHANCE_JOBS; i++) {
		HitChanceInput args;
		args.start = i * (256 / HITCHANCE_JOBS);
		args.jobID = i;
		Threading::QueueJob(RunHitChance, args);
	}

	Threading::FinishQueue();

	int sum = 0;

	for (int i = 0; i < HITCHANCE_JOBS; i++)
		sum += tempOutput[i];

	return (sum * 100) / 256 >= chance;
}

static ConVar* sv_usercmd_custom_random_seed = nullptr;
static ConVar* weapon_accuracy_nospread = nullptr;

void Spread::CompensateSpread(CUserCmd* cmd)
{
	if (!randomPopulated)
		PopulateRandomFloat();

	if (!sv_usercmd_custom_random_seed)
		sv_usercmd_custom_random_seed = cvar->FindVar(ST("sv_usercmd_custom_random_seed"));
	if (!weapon_accuracy_nospread)
		weapon_accuracy_nospread = cvar->FindVar(ST("weapon_accuracy_nospread"));

	if (weapon_accuracy_nospread && weapon_accuracy_nospread->GetBool())
		return;

	if (sv_usercmd_custom_random_seed && sv_usercmd_custom_random_seed->GetBool())
		return;

	FwBridge::activeWeapon->UpdateAccuracyPenalty();

	spreadVal = 0; //FwBridge::activeWeapon->GetSpread();
	inaccuracyVal = FwBridge::activeWeapon->GetInaccuracy();

	int randomSeed = cmd->random_seed % 256;

	float randInaccuracy = randomFl1[randomSeed] * inaccuracyVal;
	float randSpread = randomFl2[randomSeed] * spreadVal;

	vec2 spread;
	spread[0] = randomFlCos1[randomSeed] * randInaccuracy + randomFlCos2[randomSeed] * randSpread;
	spread[1] = randomFlSin1[randomSeed] * randInaccuracy + randomFlSin2[randomSeed] * randSpread;

	//Apply pitch-roll spread correction. TODO: it would be better not to modify global variables and simply return the angles
	FwBridge::lpData.angles.x += atanf(spread.Length()) * RAD2DEG;
	FwBridge::lpData.angles.z = atan2f(-spread[0], spread[1]) * RAD2DEG;
}

//We want to get the values of all random values because they stay constant ant never change.
static void PopulateRandomFloat()
{
	for (int i = 0; i < 256; i++) {
		RandomSeed(i + 1);
		randomFl1[i] = RandomFloat(0.f, 1.f);
		randomFlPi1[i] = RandomFloat(0.f, 2.f * M_PI);
		randomFl2[i] = RandomFloat(0.f, 1.f);
		randomFlPi2[i] = RandomFloat(0.f, 2.f * M_PI);
	}

	for (int i = 0; i < 256; i++)
		randomFlCos1[i] = cosf(randomFlPi1[i]);

	for (int i = 0; i < 256; i++)
		randomFlCos2[i] = cosf(randomFlPi2[i]);

	for (int i = 0; i < 256; i++)
		randomFlSin1[i] = sinf(randomFlPi1[i]);

	for (int i = 0; i < 256; i++)
		randomFlSin2[i] = sinf(randomFlPi2[i]);

	randomPopulated = true;
}

//This code should theoretically scale up to any SIMD level automatically with near perfect scaling.
static void RunHitChance(HitChanceInput* inp)
{
	MTR_SCOPED_TRACE("Spread", "RunHitChance");

	vec3_t spread[HITCHANCE_PER_JOB];
	float randInaccuracy[HITCHANCE_PER_JOB];
	float randSpread[HITCHANCE_PER_JOB];

	int start = inp->start;
	int jobID = inp->jobID;

	tempOutput[jobID] = 0;

	for (int i = 0; i < HITCHANCE_PER_JOB; i++)
		randInaccuracy[i] = randomFl1[i + start] * inaccuracyVal;

	for (int i = 0; i < HITCHANCE_PER_JOB; i++)
		randSpread[i] = randomFl2[i + start] * spreadVal;

	for (int i = 0; i < HITCHANCE_PER_JOB; i++)
		spread[i][0] = randomFlCos1[start + i] * randInaccuracy[i] + randomFlCos2[start + i] * randSpread[i];

	for (int i = 0; i < HITCHANCE_PER_JOB; i++)
		spread[i][1] = randomFlSin1[start + i] * randInaccuracy[i] + randomFlSin2[start + i] * randSpread[i];


	for (int i = 0; i < HITCHANCE_PER_JOB; i++) {
	    vec3_t dir = forward;

		vec3_t r, p;

		for (int u = 0; u < 3; u++)
			r[u] = right[u] * spread[i][0];

		for (int u = 0; u < 3; u++)
			p[u] = up[u] * spread[i][1];

		dir += r;
		dir += p;

		dir.Normalize();
		dir *= range;
		dir += startPos;

		unsigned int flags = 0;

		for (size_t i = 0; i < NumOfSIMD(MAX_HITBOXES); i++)
			flags |= hcHitboxes[i].Intersect(startPos, dir);

		tempOutput[jobID] += std::min(flags, 1u);
	}
}
