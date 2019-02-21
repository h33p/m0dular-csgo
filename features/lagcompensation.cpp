#include "lagcompensation.h"
#include "resolver.h"
#include "../core/fw_bridge.h"
#include "../core/engine.h"
#include "../core/temporary_animations.h"
#include "../core/mtr_scoped.h"

#include "../sdk/features/gamemovement.h"

constexpr float MAX_CHOKESIM = 0.5f;

int LagCompensation::quality = 0;
HistoryList<Players, BACKTRACK_TICKS>* LagCompensation::futureTrack = nullptr;

#ifdef TESTING_FEATURES
static uint64_t dynamicFlags = 0;
static HistoryList<Players, BACKTRACK_TICKS> fTrack[2];
static int prevTrack = 0;
static float chokeAverage[MAX_PLAYERS][2];
vec3_t acceleration[MAX_PLAYERS];
static HistoryList<vec3_t, 3> originsLC[MAX_PLAYERS];
static HistoryList<vec3_t, 3> velocities[MAX_PLAYERS];
static HistoryList<float, 3> simtimes[MAX_PLAYERS];

static void CheckDynamic();
static void UpdatePart1(uint64_t copyFlags);
static void UpdatePart2();
#endif

void LagCompensation::PreRun()
{
#ifdef TESTING_FEATURES
    MTR_SCOPED_TRACE("LagCompensation", "PreRun");
	CheckDynamic();

	//Loop through the players and find which players do not have to be updated (will be copied over)
	Players& p = FwBridge::playerTrack.GetLastItem(0);
	uint64_t copyFlags = 0;

	for (int i = 0; i < p.count; i++) {
		int pID = p.unsortIDs[i];
		if (~p.flags[i] & Flags::UPDATED)
			copyFlags |= ~(1ull << pID);
	}

	UpdatePart1(copyFlags);
#endif
}

void LagCompensation::Run()
{
#ifdef TESTING_FEATURES
	MTR_SCOPED_TRACE("LagCompensation", "Run");
	UpdatePart2();
#endif
}

#ifdef TESTING_FEATURES
static void CheckDynamic()
{
	dynamicFlags = 0;
	short counts[MAX_PLAYERS];
	memset(counts, 0, sizeof(counts));

	vec3_t originsLC[MAX_PLAYERS][BACKTRACK_TICKS];
	float times[MAX_PLAYERS][BACKTRACK_TICKS];
	int tCount = FwBridge::playerTrack.Count();
	for (int i = 0; i < tCount; i++) {
		Players& p = FwBridge::playerTrack.GetLastItem(i);
		for (int o = 0; o < p.count; o++) {
			int pID = p.unsortIDs[o];
			short& cnt = counts[pID];
			if (cnt > 0 && times[pID][cnt-1] == p.time[o])
				continue;
			originsLC[pID][cnt] = p.origin[o];
			times[pID][cnt] = p.time[o];
			cnt++;
		}
	}

	float velocity[BACKTRACK_TICKS];
	float timeChokedLC[BACKTRACK_TICKS];
	float chokedSum[2];
	memset(chokedSum, 0, sizeof(chokedSum));
	float chokedDiff;

	//Checks whether or not trying to break lag compensation yields lower fakelag fluxuation
	for (uint64_t i = 0; i < MAX_PLAYERS; i++) {
		vec3_t diffs[BACKTRACK_TICKS];
		float timeChoked[BACKTRACK_TICKS];
		for (int o = 1; o < counts[i]; o++) {
			diffs[o-1] = originsLC[i][o-1] - originsLC[i][o];
			timeChoked[o-1] = times[i][o-1] - times[i][o];
		}
		for (int o = 0; o < counts[i]-1; o++) {
			velocity[o] = (diffs[o] * (globalVars->interval_per_tick / timeChoked[o])).LengthSqr();
			timeChokedLC[o] = timeChoked[o] - (4096.f / velocity[o]);
			chokedSum[0] += timeChoked[o];
			chokedSum[1] += timeChokedLC[o];
		}
		for (int u = 0; u < 2; u++)
			chokedSum[u] = fabsf(chokedSum[u]);
		chokedDiff = chokedSum[1] - chokedSum[0];

		for (int u = 0; u < 2; u++)
			chokeAverage[i][u] = fmaxf(chokedSum[u] / (counts[i] - 1), globalVars->interval_per_tick);

		dynamicFlags |= ((uint64_t)(std::signbit(chokedDiff)) << i);
	}
}

static void UpdatePart1(uint64_t copyFlags)
{
	Players& p = FwBridge::playerTrack.GetLastItem(0);
	int nextTrack = (prevTrack + 1) % 2;
	auto& track = fTrack[nextTrack];
	track.Reset();

	INetChannelInfo* nci = engine->GetNetChannelInfo();
	float latency = nci ? nci->GetLatency(FLOW_OUTGOING) + nci->GetLatency(FLOW_INCOMING) - Engine::LerpTime() * 1: 0.f;
	int futureTicks = latency / globalVars->interval_per_tick;

	int cnt = 0;

	float nextSimtime[MAX_PLAYERS];
	vec3_t velocity[MAX_PLAYERS];
	int mul[MAX_PLAYERS];
	float velocityTime[MAX_PLAYERS];

	float interval = globalVars->interval_per_tick;

	uint64_t ignoreFlags = 0;

	for (int i = 0; i < p.count; i++) {
		int pID = p.unsortIDs[i];
		velocity[pID] = p.velocity[i];
		//TODO: handle this (should be handled by copy flags really)
		if (simtimes[pID][0] != p.time[i]) {
			simtimes[pID].Push(p.time[i]);
			originsLC[pID].Push(p.origin[i]);
		}
		velocities[pID].Push((vec3_t)FwBridge::playerList[pID]->velocity());
	}

	//Calculate acceleration and next simtime
	for (int i = 0; i < MAX_PLAYERS; i++) {

		if (~FwBridge::playersFl & (1ull << i) || ignoreFlags & (1ull << i))
			continue;

		acceleration[i] = (velocities[i][0] - velocities[i][1]) * (interval / (simtimes[i][0] - simtimes[i][1]));
		mul[i] = (dynamicFlags & (1ull << i)) >> i;
		mul[i] = 0;
		velocityTime[i] = mul[i] * (4096.f / fmaxf(0.001f, velocities[i][0].LengthSqr()));
		nextSimtime[i] = simtimes[i][0] + TicksToTime(TimeToTicks(fminf(velocityTime[i] + chokeAverage[i][mul[i]], MAX_CHOKESIM)));
	}

	//Run rough simulation to determine the simtimes the players should be simulated to
	//We can not accurately predict the simtimes anyways
	for (int i = 0; i < futureTicks && cnt < BACKTRACK_TICKS; i++) {
		uint64_t dirty = 0;
		int pc = 0;
		int sortIDs[MAX_PLAYERS];
		int unsortIDs[MAX_PLAYERS];

		for (int o = 0; o < MAX_PLAYERS; o++) {

			sortIDs[o] = -1;
			unsortIDs[o] = -1;

			if (~FwBridge::playersFl & (1ull << o) || ignoreFlags & (1ull << o) || !FwBridge::playerList[o])
				continue;

			velocity[o] += acceleration[o];

			if (nextSimtime[o] <= simtimes[o][0] + interval * i) {
				dirty |= (1ull << o);
				unsortIDs[pc] = o;
				sortIDs[o] = pc++;
				velocityTime[o] = mul[o] * (4096.f / fmaxf(0.001f, velocities[o][0].LengthSqr()));
				nextSimtime[o] += TicksToTime(TimeToTicks(fminf(velocityTime[o] + chokeAverage[o][mul[o]], MAX_CHOKESIM)));
			}
		}

		if (pc > 0) {
			Players& next = track.Push();
			next.Allocate(pc);
			next.globalTime = globalVars->curtime + interval * (i + 1);
			memcpy(next.sortIDs, sortIDs, sizeof(next.sortIDs));
			memcpy(next.unsortIDs, unsortIDs, sizeof(next.sortIDs));
			memset(next.flags, 0, sizeof(next.flags[0]) * next.count);
			for (int o = 0; o < next.count; o++) {
				next.time[o] = nextSimtime[unsortIDs[o]];
				next.flags[o] |= Flags::EXISTS;
				C_BasePlayer* ent = FwBridge::playerList[unsortIDs[o]];
				if (!Engine::IsEnemy(ent))
					next.flags[o] |= Flags::FRIENDLY;
			}
		}
	}

	LagCompensation::futureTrack = fTrack + nextTrack;
	prevTrack = nextTrack;
}

//Simulate target player until specified time
static void SimulateUntil(Players* p, int id, TemporaryAnimations* anim, float* curtime, Circle* path, float targetTime, bool updateAnims = false)
{
	MTR_SCOPED_TRACE("LagCompensation", "SimulateUntil");

	int pID = p->unsortIDs[id];
	C_BasePlayer* ent = FwBridge::playerList[pID];
	anim->SetTime(ent->prevSimulationTime() - *curtime);
	int fl = ent->flags();
	float ct = *curtime;
	bool onGround = ent->flags() & FL_ONGROUND;
	float accel = acceleration[pID].Length();

	vec3& velocity = ent->velocity();
	vec3& origin = ent->origin();

	int maxTicks = TimeToTicks(MAX_CHOKESIM);

	float interval = globalVars->interval_per_tick;

	while ((ct += interval) <= targetTime && maxTicks-- >= 0) {
		if (onGround)
		    ent->flags() |= FL_ONGROUND;
		else
		    ent->flags() &= ~FL_ONGROUND;

		float omega = velocity.Length<2>() * fminf(path->invRadius, 1000);
		float predAngle = path->direction * omega * interval;

		vec3_t vel = velocity;
		vel.Rotate<2>(predAngle);

		vec3_t dir = vel.Normalized();
		vel.x += accel * dir.x * interval;
		vel.y += accel * dir.y * interval;

		vec3_t torig = origin;

		if (LagCompensation::quality > 0 || vel.LengthSqr() > 10)
			SourceGameMovement::PlayerMove(ent, &torig, &vel, &onGround, true, interval);
		velocity = vel;
		origin = torig;
		interval = fmaxf(fminf(interval, ct - *curtime), globalVars->interval_per_tick);
	}

	if (updateAnims) {
		//ent->eyeAngles()[1] = Resolver::resolvedAngles[pID];
		//ent->UpdateClientSideAnimation();
		//SetAbsAngles(ent, ent->angles());
		SetAbsOrigin(ent, origin);
	}

	ent->flags() = fl;
	if (onGround)
		p->flags[id] |= Flags::ONGROUND;
	p->origin[id] = origin;
	p->velocity[id] = velocity;
	p->time[id] = targetTime;
	*curtime = targetTime;
	FwBridge::playerList[pID]->flags() = fl;
}

static std::vector<int> updatedPlayersLC;
static std::vector<int> nonUpdatedPlayersLC;

static Players* boneSetupPlayersLC = nullptr;

static vec3 originalOriginsLC[MAX_PLAYERS];

static void ThreadedPlayerReset(void* idx)
{
	MTR_SCOPED_TRACE("LagCompensation", "ThreadedPlayerReset");

	int i = (int)(uintptr_t)idx;

	SetAbsOrigin(FwBridge::playerList[i], originalOriginsLC[i]);
	Engine::UpdatePlayer(FwBridge::playerList[i], nullptr);
}

static MultiUpdateData queuedUpdateDataLC;
static std::vector<int> toQueueIDs;

static int lcQuality = 0;

bool dirtyBones[MAX_PLAYERS];

static void SimulatePlayer(void* idxVoid)
{
	MTR_SCOPED_TRACE("LagCompensation", "SimulatePlayer");
	int uID = (int)(uintptr_t)idxVoid;

	auto& track = *LagCompensation::futureTrack;
	Players* prevTrack = &FwBridge::playerTrack[0];
	C_BasePlayer* ent = FwBridge::playerList[uID];

	originalOriginsLC[uID] = FwBridge::playerList[uID]->GetClientRenderable()->GetRenderOrigin();

	float csimtime = simtimes[uID][0];
	TemporaryAnimations anims(FwBridge::playerList[uID]);
	Circle circle = Circle(originsLC[uID][0], originsLC[uID][1], originsLC[uID][2]);
	FwBridge::playerList[uID]->velocity() = prevTrack->velocity[prevTrack->sortIDs[uID]];
	vec3_t origin = originsLC[uID][0];
	vec3_t velocity = ent->velocity();

	for (int i = track.Count() - 1; i >= 0; i--) {
		Players& p = track[i];

		int sID = p.sortIDs[uID];

		if (sID >= 0 && sID < MAX_PLAYERS && ~p.flags[sID] & Flags::FRIENDLY) {
			SimulateUntil(&p, sID, &anims, &csimtime, &circle, p.time[sID], true);
			p.flags[sID] |= Flags::UPDATED;
			dirtyBones[uID] = true;
		}

		FwBridge::UpdateSinglePlayer(&p, prevTrack, false, uID);
		prevTrack = &p;
	}

	ent->origin() = origin;
	ent->velocity() = velocity;
}

static void UpdatePart2()
{
	memset(dirtyBones, 0, sizeof(dirtyBones));
	lcQuality = Settings::aimbotLagCompensation;
	Players* prevTrack = &FwBridge::playerTrack[0];

	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (FwBridge::playersFl & (1ull << i)) {
			if (!FwBridge::playerList[i])
				continue;

			int pID = prevTrack->sortIDs[i];

			if (prevTrack->flags[pID] & Flags::FRIENDLY)
				continue;

			Threading::QueueJobRef(SimulatePlayer, (void*)(uintptr_t)i);
		}
	}

	Threading::FinishQueue(true);

	for (int i = 0; i < MAX_PLAYERS; i++)
		if (dirtyBones[i])
			Threading::QueueJobRef(ThreadedPlayerReset, (void*)(uintptr_t)i);

	Threading::FinishQueue(true);
}
#endif
