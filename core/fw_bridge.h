#ifndef FW_BRIDGE_H
#define FW_BRIDGE_H
/* Here we implement game specific functions
 * to bridge the game with the framework
*/

constexpr int MAX_PLAYERS = 64;
constexpr int BACKTRACK_TICKS = 64;

#include "../sdk/framework/players.h"
#include "../sdk/framework/utils/history_list.h"
#include "../sdk/source_csgo/sdk.h"

extern CBaseClient* cl;
extern IClientMode* clientMode;
extern IVEngineClient* engine;
extern IClientEntityList* entityList;
extern CGlobalVarsBase* globalVars;
extern IVModelInfo* mdlInfo;

typedef vec3(__thiscall*Weapon_ShootPositionFn)(void*);
extern Weapon_ShootPositionFn Weapon_ShootPosition;

namespace FwBridge
{
	extern HistoryList<Players, BACKTRACK_TICKS> playerTrack;
	extern C_BaseEntity* localPlayer;
	void UpdatePlayers(CUserCmd* cmd);
	void UpdateLocalData(CUserCmd* cmd);
}

#endif
