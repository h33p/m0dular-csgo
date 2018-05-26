#ifndef FW_BRIDGE_H
#define FW_BRIDGE_H
/* Here we implement game specific functions
 * to bridge the game with the framework
*/

constexpr int MAX_PLAYERS = 64;
constexpr int BACKTRACK_TICKS = 64;

#include "../framework/players.h"
#include "../framework/utils/history_list.h"
#include "../framework/source_csgo/sdk.h"

extern CBaseClient* cl;
extern IClientMode* clientMode;
extern IVEngineClient* engine;
extern IClientEntityList* entityList;
extern CGlobalVarsBase* globalVars;
extern IVModelInfo* mdlInfo;

namespace FwBridge
{
	extern HistoryList<Players, BACKTRACK_TICKS> playerTrack;
	extern C_BaseEntity* localPlayer;
	void UpdatePlayers(CUserCmd* cmd);
	void UpdateLocalData(CUserCmd* cmd);
}

#endif
