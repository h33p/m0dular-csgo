#ifndef LEGITBOT_H
#define LEGITBOT_H

#include "../sdk/framework/utils/history_list.h"

struct AimbotTarget;
struct LocalPlayer;
struct Players;
struct CUserCmd;

namespace LegitBot
{
	bool PreRun(LocalPlayer* lpData);
	void RunPostTarget(LocalPlayer* lpData, CUserCmd* cmd, AimbotTarget* target, HistoryList<Players, 64>* track);
}

#endif
