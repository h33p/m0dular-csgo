#ifndef AIMLOG_H
#define AIMLOG_H

struct AimbotTarget;
struct LocalPlayer;

namespace AimLog
{
	void LogCreateMove(const LocalPlayer& lpData, const AimbotTarget& target);
}

#endif
