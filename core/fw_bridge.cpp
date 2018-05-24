#include "fw_bridge.h"

C_BaseEntity* FwBridge::localPlayer = nullptr;

void FwBridge::UpdatePlayers(CUserCmd* cmd)
{
	int mClients = engine->GetMaxClients();

	for (int i = 0; i < mClients && i < 64; i++) {
		C_BaseEntity* ent = (C_BaseEntity*)entityList->GetClientEntity(i);
		if (ent == localPlayer || !ent || !ent->IsPlayer())
			continue;
	}
}

void FwBridge::UpdateLocalData(CUserCmd* cmd)
{
	localPlayer = (C_BaseEntity*)entityList->GetClientEntity(engine->GetLocalPlayer());
}
