#include "glow.h"
#include "../core/engine.h"

std::set<int> indexesToRemove;
std::vector<int> prevIndexes;

static bool glowOutline = false;
static bool glowEnemy = true;
static vec4_t glowEnemyColor = vec4_t(1, 0.3, 0, 1);
static bool glowTeam = true;
static vec4_t glowTeamColor = vec4_t(0, 0.3, 1, 1);
static bool glowWeapons = true;
static vec4_t glowWeaponsColor = vec4_t(0.2, 0.5, 1, 1);
static bool glowC4 = true;
static vec4_t glowC4Color = vec4_t(1, 0.7, 0, 1);

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

	int glowStyle = 2 * (int)glowOutline;

	for (int i = 0; i < entCount; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)entityList->GetClientEntity(i);

		if (!ent || ent->IsDormant())
			continue;

		ClassId classID = ent->GetClientClass()->classID;
		vec4_t col = vec4_t(0);

		switch (classID) {
		case ClassId::ClassId_CCSPlayer:
			if (Engine::IsEnemy((C_BasePlayer*)ent)) {
				if (glowEnemy)
					col = glowEnemyColor;
			} else if (ent != Engine::localPlayer && glowTeam)
				col = glowTeamColor;
			break;
		case ClassId::ClassId_CC4:
			if (glowC4)
				col = glowC4Color;
			break;
		default:
			if (ent->IsWeapon() && glowWeapons)
				col = glowWeaponsColor;

			break;
		}

		if (col.w != 0)
			AddGlow(ent, col, glowStyle);
	}
}

static void CleanUp()
{
	for (int i : indexesToRemove)
		if (glowObjectManager->IsSlotInUse(i))
			glowObjectManager->UnregisterGlowObject(i);

}

void Glow::Run()
{
	indexesToRemove.clear();

	for (int i : prevIndexes)
		indexesToRemove.insert(i);

	prevIndexes.clear();

	if (Engine::localPlayer && engine->IsInGame())
		DoEntityLoop();
}

void Glow::Shutdown()
{
	CleanUp();
}

