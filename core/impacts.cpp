#include "impacts.h"
#include "fw_bridge.h"
#include "engine.h"
#include "visuals.h"
#include "resolver.h"
#include "temporary_animations.h"
#include "lagcompensation.h"
#include "../sdk/framework/utils/history_list.h"
#include "../sdk/framework/utils/intersect_impl.h"
#include "../sdk/features/fakelag.h"

/*
  Shot data is contained in both game events and effects.
  Some specific data, such as attacker and world space impact position are provided in the event,
  while other important data, such as hitbox, damage, local impact position are provided in the effect.
  Also, events are purely server sided, while effects can be client-side as well (shooting walls emits a client side effect).
  The event is called first, then comes the effect.
  Combining both will result us in having completely enough information for any impact based resolving.
*/

struct PlayerBackup
{
	vec3_t origin[MAX_PLAYERS];
	//This is useless for now
	//AnimationLayer layers[MAX_PLAYERS][13];
};

static HistoryList<BulletData, 20> eventsQueue;
static HistoryList<BulletData, 20> worldImpacts;
static HistoryList<BulletData, 20> localImpacts;
static uint64_t hitFlags = 0;
static uint64_t onGround = 0;
static HistoryList<vec3_t, BACKTRACK_TICKS> prevShootOrigins;

//There are problems in debug mode when loading having a large chunk of memory allocated in the data segment.
static HistoryList<PlayerBackup, BACKTRACK_TICKS> historyStates;

static bool effectCalled = false;

static void ProcessHitEntity(BulletData data);
static void ProcessWorldImpacts();
static void ProcessLocalImpacts(bool hitShot, int hitbox);
static void ProcessOtherHits();
static void ProcessBulletQueue();

void Impacts::Tick()
{
	Players& players = FwBridge::playerTrack.GetLastItem(0);

	ProcessBulletQueue();
	prevShootOrigins.Push(FwBridge::lpData.eyePos);

	PlayerBackup backup;
	for (int i = 0; i < players.count; i++) {
		int id = players.unsortIDs[i];
		if (id < 0 || id >= MAX_PLAYERS)
			continue;
		C_BasePlayer* ent = (C_BasePlayer*)players.instance[i];
		if (!ent)
			continue;
		//memcpy(backup.layers[id], ent->animationLayers(), sizeof(backup.layers[id]));
		backup.origin[id] = (vec3)ent->GetClientRenderable()->GetRenderOrigin();
	}
	historyStates.Push(backup);
}

void Impacts::ImpactEvent(IGameEvent* data, unsigned int crc)
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
	event.backTick = -1;
	event.attacker = engine->GetPlayerForUserID(data->GetInt(ST("userid")));
	event.pos.x = data->GetFloat(ST("x"));
	event.pos.y = data->GetFloat(ST("y"));
	event.pos.z = data->GetFloat(ST("z"));
	event.addTime = FwBridge::lpData.time + Engine::LerpTime() + globalVars->interval_per_tick;
	event.onGround = -1;
	event.processed = false;

	eventsQueue.Push(event);
}

void Impacts::HandleImpact(const CEffectData& effectData)
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
		int backTick = 0;

		for (size_t i = 0; i < eventsQueue.Count(); i++) {
			BulletData& data = eventsQueue.GetLastItem(i);

			if (data.addTime < FwBridge::lpData.time)
				break;

			if (data.cleared)
				continue;

			for (size_t o = 0; o < FwBridge::playerTrack.Count() && o < 32; o++) {
				Players& players = FwBridge::playerTrack.GetLastItem(o);

				int pID = players.sortIDs[entID];
				if (pID >= MAX_PLAYERS)
					continue;

				int hbID = FwBridge::hitboxIDs[effectData.hitBox];

				vec3_t addVec(0, 0, 0);
				if (hbID >= 0 && hbID < MAX_HITBOXES)
					addVec.z = -effectOrigin.z -players.origin[pID][2] + players.hitboxes[pID].wm[hbID][2][3];

				float dist = (players.origin[pID] + addVec + effectOrigin - data.pos).Length();

				//This could be a nearby client impact effect (TODO: detect client-side impacts)
				if (!hbID && fabs(players.origin[pID][2] + effectOrigin.z - players.hitboxes[pID].wm[0][2][3]) > 30.f)
					continue;

				if (dist >= closestDistance)
					continue;

				finalData = &data;
				closestDistance = dist;
				backTick = o;
			}
		}

		if (finalData) {
			Players& players = FwBridge::playerTrack.GetLastItem(backTick);

			finalData->relPos = effectOrigin;
			finalData->relStart = (vec3)effectData.start;
			finalData->hitEnt = entID;
			finalData->hitbox = effectData.hitBox;
			finalData->backTick = backTick;
			finalData->onGround = players.flags[players.sortIDs[entID]] & Flags::ONGROUND;
			finalData->cleared = true;
		}
	}
}


static float resolvedAngle[MAX_PLAYERS];

static void ProcessBulletQueue()
{

	if (!eventsQueue.Count() && !effectCalled)
		return;

	hitFlags = 0;
	onGround = 0;

	worldImpacts.Reset();
	localImpacts.Reset();

	//Did we hit a player?
	bool localHit = false;
	int hitbox = -1;
	//Do we need to do any processing?
	bool hasEvents = false;
	//Is there any events queued up in the future for the effects to process?
	bool shouldClear = true;

	for (size_t i = 0; i < eventsQueue.Count(); i++) {
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

		if (data.attacker == FwBridge::lpData.ID) {
			localHit = true;
			hitbox = data.hitbox;
		}

		if (data.hitEnt >= 0)
			ProcessHitEntity(data);
		else
			worldImpacts.Push(data);
	}

	if (hasEvents) {
		ProcessWorldImpacts();
		ProcessLocalImpacts(localHit, hitbox);
		ProcessOtherHits();

		effectCalled = false;
	}

	if (shouldClear)
		eventsQueue.Reset();
}

static void ProcessHitEntity(BulletData data)
{
	hitFlags |= (1ull << data.hitEnt);

	//Ignore hitboxes that lead to inaccurate results or are not in the hitbox list
	switch(data.hitbox)
	{
	  case Hitboxes::HITBOX_LEFT_UPPER_ARM:
	  case Hitboxes::HITBOX_RIGHT_UPPER_ARM:
	  case Hitboxes::HITBOX_UPPER_CHEST:
	  case Hitboxes::HITBOX_RIGHT_CALF:
	  case Hitboxes::HITBOX_RIGHT_THIGH:
	  case Hitboxes::HITBOX_RIGHT_FOREARM:
	  case Hitboxes::HITBOX_PELVIS:
	  case Hitboxes::HITBOX_STOMACH:
	  case Hitboxes::HITBOX_LOWER_CHEST:
	  case Hitboxes::HITBOX_CHEST:
	  //case Hitboxes::HITBOX_LOWER_NECK:
		  return;
	  case Hitboxes::HITBOX_HEAD:
	  case Hitboxes::HITBOX_NECK:
		  break;
	  default:
		  return;
	}

	C_BasePlayer* ent = (C_BasePlayer*)entityList->GetClientEntity(data.hitEnt);

	if (!ent || ent->IsDormant())
		return;

	//TODO: Handle this case by rotating the hitbox around the axis
	if (ent->lifeState() != LIFE_ALIVE)
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

	int lerpTicks = std::min((int)(Engine::LerpTime() / globalVars->interval_per_tick), 3) + data.backTick;

	//Backup player data
	vec3_t entBOrigin = (vec3)ent->GetClientRenderable()->GetRenderOrigin();
	float eyeYaw = ent->eyeAngles()[1];
	vec3 angsBackup = ent->angles();
	matrix3x4_t matrix[128];
	TemporaryAnimations anims(ent, -lerpTicks);

	PlayerBackup& backup = historyStates.GetLastItem(lerpTicks);
	vec3_t entOrigin = backup.origin[data.hitEnt];
	SetAbsOrigin(ent, entOrigin);

	vec3_t start = data.relStart + entOrigin;
	vec3_t rend = data.relPos + entOrigin;
	vec3_t end = rend + (rend - start).Normalized() * hitbox->radius * 0.2f;

	float hitYaw = data.relPos.GetAngles(true)[1];
	float shootAngles = (rend - start).GetAngles(true)[1];
	float goalFeetYawDelta = NormalizeFloat(animState->goalFeetYaw - eyeYaw, -180.f, 180.f);
	float rotationDelta = NormalizeFloat(ent->angles()[1] - eyeYaw, -180.f, 180.f);

	Visuals::PassStart(start, rend);
	ent->clientSideAnimation() = true;

	float closestDist = 80.f;
	float bestAngle = 0.f;

	ent->UpdateClientSideAnimation();
	Engine::UpdatePlayer(ent, matrix);

	vec3_t s(0);
	float boneOffset = (matrix[hitbox->bone].Vector3Transform(s) - entOrigin).GetAngles(true)[1] - eyeYaw;

	//Get the direction we should move towards
	float sign = NormalizeFloat(shootAngles - hitYaw + boneOffset, -180.f, 180.f);
	sign /= fabsf(sign);

	for (int i = 0; i < 7; i++) {
		CapsuleColliderSOA<16> collider;
		zvec3 closestDir;
		zvec3 hitboxPos;

		for (int o = 0; o < 16; o++)
			collider.radius[o] = hitbox->radius;

		for (int o = 0; o < 16; o++) {
			animState->prevOrigin = entOrigin;
			animState->origin = entOrigin;
			ent->eyeAngles()[1] = NormalizeFloat(hitYaw - boneOffset - sign * (i * 16 + o) + 96.f * sign, -180.f, 180.f);
			animState->goalFeetYaw = NormalizeFloat(goalFeetYawDelta + ent->eyeAngles()[1], 0.f, 360.f);
			ent->UpdateClientSideAnimation();
			vec3 angs = ent->angles();
			angs[1] = NormalizeFloat(rotationDelta + ent->eyeAngles()[1], 0.f, 360.f);
			SetAbsAngles(ent, angs);
			anims.RestoreState();
			Engine::UpdatePlayer(ent, matrix);

			collider.start.acc[o] = matrix[hitbox->bone].Vector3Transform(hitbox->bbmin);
			collider.end.acc[o] = matrix[hitbox->bone].Vector3Transform(hitbox->bbmax);

			for (int u = 0; u < 3; u++)
				hitboxPos[u][1] = matrix[hitbox->bone][u][3];

			SetAbsAngles(ent, angsBackup);
			ent->eyeAngles()[1] = eyeYaw;
		}

		Visuals::PassColliders(collider.start, collider.end);
		unsigned int flags = collider.Intersect(start, end, &closestDir);

		if (flags) {
			int idx = 31 - Clz(flags);
			closestDist = 0.f;
			bestAngle = NormalizeFloat(hitYaw - boneOffset - sign * (i * 16 + idx) + 96.f * sign, -180.f, 180.f);
			Visuals::PassBest(idx, i);
			break;
		}
	}

	SetAbsOrigin(ent, entBOrigin);
	ent->clientSideAnimation() = false;
	if (closestDist < 80.f) {
		cvar->ConsoleDPrintf(ST("Resolved from shot: %f %d\n"), NormalizeFloat(bestAngle, -180.f, 180.f), data.hitbox);
	    hitFlags |= (1ull << data.hitEnt);
		if (data.onGround > 0)
			onGround |= (1ull << data.hitEnt);
		resolvedAngle[data.hitEnt] = NormalizeFloat(bestAngle, 0.f, 360.f);
	}

}

static void ProcessLocalImpacts(bool hitShot, int hitbox)
{
	//Find the longest ray from the list and perform the spread check on the correct aimbot target

	if (localImpacts.Count() <= 0)
		return;

	INetChannelInfo* nci = engine->GetNetChannelInfo();
	float ping = nci ? nci->GetAvgLatency(FLOW_OUTGOING) + nci->GetLatency(FLOW_INCOMING) + Engine::LerpTime(): 0.f;
	int pingTicks = ping / globalVars->interval_per_tick;
	int margin = 0.05f / globalVars->interval_per_tick + SourceFakelag::prevChokeCount;
	AimbotTarget* aimbotTarget = nullptr;
	unsigned int targetIntersects = 0;
	int pb = 1024;
	int pbi = 0;

	for (int i = std::max(pingTicks - margin, 0); i < pingTicks + margin; i++) {
		AimbotTarget& target = FwBridge::aimbotTargets.GetLastItem(i);

		if (target.id < 0 || std::abs(i - pingTicks) > pb)
			continue;

		aimbotTarget = &target;
		targetIntersects = FwBridge::aimbotTargetIntersects[i];
		pb = std::abs(i - pingTicks);
		pbi = i;
	}

	if (!aimbotTarget)
		return;

	float longestDist = 0.f;
	vec3_t startPoint = prevShootOrigins.GetLastItem(pbi);
	vec3_t endPoint;

	for (size_t i = 0; i < localImpacts.Count(); i++) {
		BulletData& data = localImpacts.GetLastItem(i);
		float len = (data.pos - startPoint).LengthSqr();
		if (len > longestDist) {
			endPoint = data.pos;
			longestDist = len;
		}
	}

	//This should never really be negative, but still handle it just in case
	int simtick = pbi + aimbotTarget->backTick + 1;
	Players& players = simtick >= 0 ? FwBridge::playerTrack.GetLastItem(simtick) : LagCompensation::futureTrack->GetLastItem(-simtick);
	CapsuleColliderSOA<SIMD_COUNT>* colliders = players.colliders[aimbotTarget->id];

	unsigned int flags = 0;

	vec3_t endPointPlus = endPoint + (endPoint - startPoint).Normalized() * 50.f;

	for (size_t i = 0; i < NumOfSIMD(MAX_HITBOXES); i++)
	    flags |= (colliders[i].Intersect(startPoint, endPointPlus) << (i * SIMD_COUNT));

	Color colorOK = Color(0, 255, 0, 255);
	Color colorMiss = Color(255, 0, 0, 255);

	//TODO: Fix this check, for now we only have accurate hit/miss check for the head hitbox, on other hitboxes -- simply checking if we hit the player, otherwise, we count as hit
	if (hitShot && (hitbox < 0 || ((aimbotTarget->boneID == 0 && ~targetIntersects & (1u << FwBridge::hitboxIDs[hitbox])) && aimbotTarget->boneID != FwBridge::hitboxIDs[hitbox])))
		hitShot = false;

	Resolver::ShootPlayer(players.unsortIDs[aimbotTarget->id], players.flags[aimbotTarget->id] & Flags::ONGROUND);

	int unsortID = players.unsortIDs[aimbotTarget->id];

	if (!hitShot) {
		if (!flags)
			cvar->ConsoleColorPrintf(colorMiss, ST("Missed shot due to spread.\n"));
		else
			cvar->ConsoleColorPrintf(colorMiss, ST("Missed shot due to incorrect animefix.\n"));
	} else {

		C_BasePlayer* instance = (C_BasePlayer*)players.instance[aimbotTarget->id];

		if (hitFlags & (1ull << unsortID)) {
			Resolver::HitPlayer(unsortID, players.flags[aimbotTarget->id] & Flags::ONGROUND, resolvedAngle[unsortID]);
		    hitFlags &= ~(1ull << unsortID);
		} else if (instance)
			Resolver::HitPlayer(unsortID, players.flags[aimbotTarget->id] & Flags::ONGROUND, instance->eyeAngles()[1]);

		if (!flags)
			cvar->ConsoleColorPrintf(colorOK, ST("Hit shot due to spread!\n"));
		else
			cvar->ConsoleColorPrintf(colorOK, ST("Hit shot!\n"));

	}

#ifdef PT_VISUALS
	if (!flags && aimbotTarget)
		Visuals::SetShotVectors(startPoint, endPoint, startPoint, aimbotTarget->targetVec);
#endif
}

static void ProcessOtherHits()
{
	for (int i = 0; i < MAX_PLAYERS && i < 64; i++)
		if (hitFlags & (1ull << i))
			Resolver::HitPlayer(i, onGround & (1ull << i), resolvedAngle[i]);
}

static void ProcessWorldImpacts()
{

}
