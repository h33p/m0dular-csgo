#include "resolver.h"
#include "fw_bridge.h"
#include "engine.h"
#include "visuals.h"
#include "../sdk/framework/utils/history_list.h"
#include "../sdk/framework/utils/intersect_impl.h"

/*
  Shot data is contained in both game events and effects.
  Some specific data, such as attacker and world space impact position are provided in the event,
  while other important data, such as hitbox, damage, local impact position are provided in the effect.
  Also, events are purely server sided, while effects can be client-side as well (shooting walls emits a client side effect).
  The event is called first, then comes the effect.
  Combining both will result us in having completely enough information for any impact based resolving.
*/

struct BulletData
{
	vec3_t pos;
	vec3_t relPos;
	vec3_t relStart;
	int attacker;
	int hitEnt;
	int hitbox;
	float addTime;

	bool cleared;
	float processed;
};

struct PlayerBackup
{
	CCSGOPlayerAnimState animState;
	AnimationLayer layers[13];
};

static HistoryList<BulletData, 20> eventsQueue;
static HistoryList<BulletData, 20> worldImpacts;
static HistoryList<BulletData, 20> localImpacts;
static unsigned long long hitFlags = 0;
static HistoryList<vec3_t, BACKTRACK_TICKS> prevShootOrigins;

//There are problems in debug mode when loading having a large chunk of memory allocated in the data segment.
auto* historyStates = new HistoryList<PlayerBackup, 3>[MAX_PLAYERS];

static bool effectCalled = false;

static void ProcessHitEntity(BulletData data);
static void ProcessWorldImpacts();
static void ProcessLocalImpacts(bool hitShot);
static void ProcessBulletQueue();

void Resolver::Tick()
{
	Players& players = FwBridge::playerTrack.GetLastItem(0);

	ProcessBulletQueue();
	prevShootOrigins.Push(FwBridge::lpData.eyePos);

	for (int i = 0; i < players.count; i++) {
		int id = players.unsortIDs[i];
		if (id < 0 || id >= MAX_PLAYERS)
			continue;
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		if (!ent)
			continue;
		PlayerBackup backup;
		backup.animState = *ent->animState();
		memcpy(backup.layers, ent->animationLayers(), sizeof(backup.layers));
		historyStates[id].Push(backup);
	}
}

void Resolver::ImpactEvent(IGameEvent* data, unsigned int crc)
{
	if (crc != CCRC32("bullet_impact"))
		return;

	if (effectCalled)
		ProcessBulletQueue();

	effectCalled = false;

	BulletData event;

	event.cleared = false;
	event.hitEnt = -1;
	event.hitbox = -1;
	event.attacker = engine->GetPlayerForUserID(data->GetInt(ST("userid")));
	event.pos.x = data->GetFloat(ST("x"));
	event.pos.y = data->GetFloat(ST("y"));
	event.pos.z = data->GetFloat(ST("z"));
	event.addTime = FwBridge::lpData.time + Engine::LerpTime();
	event.processed = false;

	eventsQueue.Push(event);
}

void Resolver::HandleImpact(const CEffectData& effectData)
{
	if (effectData.entity == -1)
		return;

	C_BasePlayer* ent = (C_BasePlayer*)entityList->GetClientEntityFromHandle(effectData.entity);

	if (!ent)
		return;

	effectCalled = true;
	int entID = ent->EntIndex();
	vec3_t effectOrigin = (vec3)effectData.origin;

	if (!entID && ent->GetClientClass()->classID != ClassId::ClassId_CCSPlayer) {
		//The impact hit the world, position is absolute
	} else if (ent->GetClientClass()->classID == ClassId::ClassId_CCSPlayer) {
		//Find the closest matching player from the event list and player track
		BulletData* finalData = nullptr;
		float closestDistance = 20.f;

		for (int i = 0; i < eventsQueue.Count(); i++) {
			BulletData& data = eventsQueue.GetLastItem(i);

			if (data.addTime < FwBridge::lpData.time)
				break;

			if (data.cleared)
				continue;

			for (int o = 0; o < FwBridge::playerTrack.Count(); o++) {
				Players& players = FwBridge::playerTrack.GetLastItem(o);

				int pID = players.sortIDs[entID];
				if (pID >= MAX_PLAYERS)
					continue;

				float dist = (players.origin[pID] + effectOrigin - data.pos).Length();

				if (dist >= closestDistance)
					continue;

				finalData = &data;
				closestDistance = dist;
			}
		}

		if (finalData) {
			finalData->relPos = effectOrigin;
			finalData->relStart = (vec3)effectData.start;
			finalData->hitEnt = entID;
			finalData->hitbox = effectData.hitBox;
			finalData->cleared = true;
		}
	}
}


static void ProcessBulletQueue()
{
	if (!eventsQueue.Count() && !effectCalled)
		return;

	hitFlags = 0;
	worldImpacts.Reset();
	localImpacts.Reset();

	//Did we hit a player?
	bool localHit = false;
	//Do we need to do any processing?
	bool hasEvents = false;
	//Is there any events queued up in the future for the effects to process?
	bool shouldClear = true;

	for (int i = 0; i < eventsQueue.Count(); i++) {
		BulletData& data = eventsQueue.GetLastItem(i);

		if (data.processed)
			break;

		if (data.addTime < FwBridge::lpData.time)
			hasEvents = true;
		else {
			shouldClear = false;
			continue;
		}

		data.processed = true;

		if (!data.cleared) {
			if (data.attacker == FwBridge::lpData.ID)
				localImpacts.Push(data);
			continue;
		}

		if (data.attacker == FwBridge::lpData.ID)
			localHit = true;

		if (data.hitEnt >= 0)
			ProcessHitEntity(data);
		else
			worldImpacts.Push(data);
	}

	if (hasEvents) {
		ProcessWorldImpacts();
		ProcessLocalImpacts(localHit);

		effectCalled = false;
	}

	if (shouldClear)
		eventsQueue.Reset();
}

static void ProcessHitEntity(BulletData data)
{
	hitFlags |= (1ull << data.hitEnt);

	C_BasePlayer* ent = (C_BasePlayer*)entityList->GetClientEntity(data.hitEnt);

	if (!ent || ent->IsDormant())
		return;

	studiohdr_t* hdr = mdlInfo->GetStudiomodel(ent->GetModel());
	if (!hdr)
	    return;

	mstudiohitboxset_t* set = hdr->GetHitboxSet(0);
	if (!set)
	    return;

	mstudiobbox_t* hitbox = set->GetHitbox(data.hitbox);

	if (!hitbox)
		return;

	CCSGOPlayerAnimState* animState = ent->animState();

	if (!animState)
		return;

	vec3_t entOrigin = (vec3)ent->GetClientRenderable()->GetRenderOrigin();

	vec3_t start = data.relStart + entOrigin;
	vec3_t end = data.relPos + entOrigin;

	float eyeYaw = ent->eyeAngles()[1];
	float currentFeetYawDelta = NormalizeFloat(animState->currentFeetYaw - eyeYaw, -180.f, 180.f);
	float hitYaw = data.relPos.GetAngles(true)[1];
	vec3 angsBackup = ent->angles();
	AnimationLayer animationLayers[13];
	matrix3x4_t matrix[128];
	CCSGOPlayerAnimState animStateBackup = *animState;
	memcpy(animationLayers, ent->animationLayers(), sizeof(AnimationLayer) * 13);

	float curtime = globalVars->curtime;
	float frametime = globalVars->frametime;
	int framecount = globalVars->framecount;
	int lerpTicks = std::min((int)(Engine::LerpTime() / globalVars->interval_per_tick), 3);

	globalVars->curtime = ent->prevSimulationTime() + globalVars->interval_per_tick - lerpTicks;
	globalVars->frametime = globalVars->interval_per_tick;
	globalVars->framecount = animState->frameCount + 1;

	Visuals::PassStart(start, end);
	ent->clientSideAnimation() = true;
	for (int i = 0; i < 11; i++) {
		CapsuleColliderSOA<16> collider;

		for (int o = 0; o < 16; o++)
			collider.radius[o] = hitbox->radius;

		for (int o = 0; o < 16; o++) {
		    PlayerBackup& backup = historyStates[data.hitEnt].GetLastItem(lerpTicks);
			*animState = backup.animState;
			memcpy(ent->animationLayers(), backup.layers, sizeof(AnimationLayer) * 13);
			ent->eyeAngles()[1] = NormalizeFloat(hitYaw - 90.f + 2 * (i * 16 + o), -180.f, 180.f);
			animState->currentFeetYaw = NormalizeFloat(currentFeetYawDelta + ent->eyeAngles()[1], 0.f, 360.f);
			ent->UpdateClientSideAnimation();
			vec3 angs = ent->angles();
			angs[1] = animState->currentFeetYaw;
			SetAbsAngles(ent, angs);
			Engine::UpdatePlayer(ent, matrix);

			collider.start.acc[o] = matrix[hitbox->bone].Vector3Transform(hitbox->bbmin);
			collider.end.acc[o] = matrix[hitbox->bone].Vector3Transform(hitbox->bbmax);

			SetAbsAngles(ent, angsBackup);
			ent->eyeAngles()[1] = eyeYaw;
		}

		memcpy(ent->animationLayers(), animationLayers, sizeof(AnimationLayer) * 13);
		*animState = animStateBackup;
		Visuals::PassColliders(collider.start, collider.end);
		unsigned int flags = collider.Intersect(start, end);

		if (flags) {
			int idx = 31 - CLZ(flags);
			float resAng = NormalizeFloat(hitYaw - 90.f + 2 * (i * 16 + idx), -180.f, 180.f);
			cvar->ConsoleDPrintf(ST("Resolved from shot: %f\n"), resAng);
			break;
		}
	}
	ent->clientSideAnimation() = false;

	globalVars->curtime = curtime;
	globalVars->frametime = frametime;
	globalVars->framecount = framecount;
}

static void ProcessLocalImpacts(bool hitShot)
{
	//Find the longest ray from the list and perform the spread check on the correct aimbot target

	if (localImpacts.Count() <= 0)
		return;

	INetChannelInfo* nci = engine->GetNetChannelInfo();
	float ping = nci ? nci->GetAvgLatency(FLOW_OUTGOING) + nci->GetLatency(FLOW_INCOMING) + Engine::LerpTime(): 0.f;
	int pingTicks = ping / globalVars->interval_per_tick;
	int margin = 0.05f / globalVars->interval_per_tick;
	Target* aimbotTarget = nullptr;
	int pb = 1024;
	int pbi = 0;

	for (int i = std::max(pingTicks - margin, 0); i < pingTicks + margin; i++) {
		Target& target = FwBridge::aimbotTargets.GetLastItem(i);

		if (target.id < 0 || std::abs(i - pingTicks) > pb)
			continue;

		aimbotTarget = &target;
		pb = std::abs(i - pingTicks);
		pbi = i;
	}

	if (!aimbotTarget)
		return;

	float longestDist = 0.f;
	vec3_t startPoint = prevShootOrigins.GetLastItem(pbi);
	vec3_t endPoint;

	for (int i = 0; i < localImpacts.Count(); i++) {
		BulletData& data = localImpacts.GetLastItem(i);
		float len = (data.pos - startPoint).LengthSqr();
		if (len > longestDist) {
			endPoint = data.pos;
			longestDist = len;
		}
	}

    CapsuleColliderSOA<SIMD_COUNT>* colliders = FwBridge::playerTrack.GetLastItem(pbi).colliders[aimbotTarget->id];

	unsigned int flags = 0;

	for (size_t i = 0; i < NumOfSIMD(MAX_HITBOXES); i++)
	    flags |= (colliders[i].Intersect(startPoint, endPoint) << (i * SIMD_COUNT));

	Color colorOK = Color(0, 255, 0, 255);
	Color colorMiss = Color(255, 0, 0, 255);

	if (!hitShot) {
		if (!flags)
			cvar->ConsoleColorPrintf(colorMiss, ST("Missed shot due to spread.\n"));
		else
			cvar->ConsoleColorPrintf(colorMiss, ST("Missed shot due to incorrect resolver.\n"));
	} else {
		if (!flags)
			cvar->ConsoleColorPrintf(colorOK, ST("Hit shot due to spread!\n"));
		else
			cvar->ConsoleColorPrintf(colorOK, ST("Hit resolved shot!\n"));
	}
}

static void ProcessWorldImpacts()
{

}
