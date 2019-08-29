#include "glow.h"
#include "../core/fw_bridge.h"
#include "../core/engine.h"
#include "../core/settings.h"

std::set<int> indexesToRemove;
std::vector<int> prevIndexes;

static void AddGlow(IHandleEntity* ent, vec4_t color, int glowStyle)
{
	int glowIndex = glowObjectManager->GetDefinitionIndex(ent);

	if (glowIndex == -1)
		glowIndex = glowObjectManager->RegisterGlowObject(ent);

	GlowObjectDefinition_t& def = glowObjectManager->glowObjectDefinitions[glowIndex];

	if (false && glowIndex != 0) {
		GlowObjectDefinition_t bac = def;
		memcpy(&def, &glowObjectManager->glowObjectDefinitions[0], sizeof(GlowObjectDefinition_t));
		def.ent = bac.ent;
		def.nextFreeSlot = bac.nextFreeSlot;
	}
	def.color = color;
	def.movementAlpha = 0;
	def.bloomAmount = 1;
	def.renderWhenOccluded = true;
	def.renderWhenUnoccluded = false;
	def.fullBloom = false;
	def.fullBloomStencilTestValue = 0;
	def.glowStyle = glowStyle;
	def.splitScreenSlot = -1;
	indexesToRemove.erase(glowIndex);
	prevIndexes.push_back(glowIndex);
}

static void DoEntityLoop()
{

	int entCount = entityList->GetHighestEntityIndex();

	int glowStyle = 2 * (int)Settings::glowOutline;

	for (int i = 0; i < entCount; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)entityList->GetClientEntity(i);

		if (!ent || ent->IsDormant())
			continue;

		ClassId classID = ent->GetClientClass()->classID;
		vec4_t col = vec4_t(0);

		switch (classID) {
		case ClassId::ClassId_CCSPlayer:
			if (Engine::IsEnemy((C_BasePlayer*)ent)) {
				if (Settings::glowEnemy)
					col = Settings::glowEnemyColor;
			} else if (ent != FwBridge::localPlayer && Settings::glowTeam)
				col = Settings::glowTeamColor;
			break;
		case ClassId::ClassId_CC4:
			if (Settings::glowC4)
				col = Settings::glowC4Color;
			break;
		default:
			if (ent->IsWeapon() && Settings::glowWeapons)
				col = Settings::glowWeaponsColor;

			break;
		}

		if (col.w != 0)
			AddGlow(ent, col, glowStyle);
	}
}

void Glow::Run()
{
	indexesToRemove.clear();

	for (int i : prevIndexes)
		indexesToRemove.insert(i);

	prevIndexes.clear();

	if (Settings::glow && FwBridge::localPlayer && engine->IsInGame())
		DoEntityLoop();

	for (int i : indexesToRemove)
		if (glowObjectManager->IsSlotInUse(i)) {
			cvar->ConsoleDPrintf("Erasing %d\n", i);
			glowObjectManager->UnregisterGlowObject(i);
		}
}

void Glow::Shutdown()
{

}