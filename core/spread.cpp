#include "spread.h"
#include "../sdk/framework/utils/threading.h"
#include "../sdk/framework/utils/intersect.h"

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
constexpr int HITCHANCE_SIMD = NumOfSIMD(HITCHANCE_PER_JOB);

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

struct HitChanceInput
{
	int start;
	int jobID;
};

static int tempOutput[HITCHANCE_JOBS];

static Players* players;
static int targetEnt;
static vec3_t targetVec;
static vec3_t forward;
static vec3_t up;
static vec3_t right;
static float inaccuracyVal;
static float spreadVal;
static float range;
static nvec3 startPos;
CapsuleCollider hitbox;

//This code should theoretically scale up to any SIMD level automatically with near perfect scaling.
static void RunHitChance(HitChanceInput* inp)
{
	nvec3 spread[HITCHANCE_SIMD];
	float randInaccuracy[HITCHANCE_PER_JOB];
	float randSpread[HITCHANCE_PER_JOB];

	int start = inp->start;
	int jobID = inp->jobID;

	tempOutput[jobID] = 0;

	for (int i = 0; i < HITCHANCE_PER_JOB; i++)
		randInaccuracy[i] = randomFl1[i + start] * inaccuracyVal;

	for (int i = 0; i < HITCHANCE_PER_JOB; i++)
		randSpread[i] = randomFl2[i + start] * spreadVal;

	for (int i = 0; i < HITCHANCE_SIMD; i++)
		for (int o = 0; o < SIMD_COUNT; o++)
			spread[i][0][o] = randomFlCos1[start + i * SIMD_COUNT + o] * randInaccuracy[i * SIMD_COUNT + o] +
				randomFlCos2[start + i * SIMD_COUNT + o] * randSpread[i * SIMD_COUNT + o];

	for (int i = 0; i < HITCHANCE_SIMD; i++)
		for (int o = 0; o < SIMD_COUNT; o++)
			spread[i][1][o] = randomFlSin1[start + i * SIMD_COUNT + o] * randInaccuracy[i * SIMD_COUNT + o] +
				randomFlSin2[start + i * SIMD_COUNT + o] * randSpread[i * SIMD_COUNT + o];


	for (int i = 0; i < HITCHANCE_SIMD; i++) {
	    nvec3 dir;

		for (int o = 0; o < SIMD_COUNT; o++)
			dir.acc[o] = forward;

	    nvec3 r, p;

		for (int u = 0; u < 3; u++)
			for (int o = 0; o < SIMD_COUNT; o++)
				r[u][o] = right[u] * spread[i][0][u];

		for (int u = 0; u < 3; u++)
			for (int o = 0; o < SIMD_COUNT; o++)
				p[u][o] = up[u] * spread[i][1][u];

		dir += r;
		dir += p;

		dir.Normalize();
		dir *= range;
		dir += startPos;

		unsigned int flags = 0;

		hitbox.IntersectSOA(startPos, dir, flags);

#ifdef _MSC_VER
		tempOutput[jobID] += __popcnt(flags);
#else
		tempOutput[jobID] += __builtin_popcount(flags);
#endif
	}
}

bool Spread::HitChance(Players* players, int targetEnt, vec3_t targetVec, int boneID)
{
	if (!randomPopulated)
		PopulateRandomFloat();

	FwBridge::activeWeapon->UpdateAccuracyPenalty();

	::players = players;
	::targetEnt = targetEnt;
	::targetVec = targetVec;

	range = FwBridge::lpData.weaponRange;
	spreadVal = FwBridge::activeWeapon->GetSpread();
	inaccuracyVal = FwBridge::activeWeapon->GetInaccuracy();
	vec3_t dir = targetVec - FwBridge::lpData.eyePos;
	startPos = FwBridge::lpData.eyePos;
	dir.Normalize().ToAngles().GetVectors(forward, up, right);

	auto& hitboxes = players->hitboxes[targetEnt];
	hitbox.start = hitboxes.wm[boneID].Vector3Transform(hitboxes.start[boneID]);
	hitbox.end = hitboxes.wm[boneID].Vector3Transform(hitboxes.end[boneID]);
	hitbox.radius = hitboxes.radius[boneID];

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

	return (sum * 100) / 256 > 50;
}
