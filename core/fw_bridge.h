#ifndef FW_BRIDGE_H
#define FW_BRIDGE_H
/* Here we implement game specific functions
 * to bridge the game with the framework
*/

constexpr int MAX_PLAYERS = 64;

#include "../framework/players.h"
#include "../framework/source_csgo/sdk.h"

extern IClientMode* clientMode;
extern IVEngineClient* engine;
extern IClientEntityList* entityList;

namespace FwBridge
{
	extern C_BaseEntity* localPlayer;
	void UpdatePlayers(CUserCmd* cmd);
	void UpdateLocalData(CUserCmd* cmd);
}

#endif
