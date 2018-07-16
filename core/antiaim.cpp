#include "antiaim.h"
#include "fw_bridge.h"

void Antiaim::Run(CUserCmd* cmd, FakelagState state)
{
	if (cmd->buttons & (IN_ATTACK | IN_ATTACK2 | IN_USE))
		return;

	LocalPlayer& lp = FwBridge::lpData;

	lp.angles.x = 89;

}
