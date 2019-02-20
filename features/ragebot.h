#ifndef RAGEBOT_H
#define RAGEBOT_H

#include "../sdk/framework/utils/history_list.h"

struct AimbotTarget;
struct LocalPlayer;
struct Players;
struct CUserCmd;

namespace RageBot
{
	bool PreRun(LocalPlayer* lpData);
	void RunPostTarget(LocalPlayer* lpData, CUserCmd* cmd, AimbotTarget* target, HistoryList<Players, 64>* track);
}

#endif
