#ifndef ANTIAIM_H
#define ANTIAIM_H

#include "../sdk/features/types.h"

struct CUserCmd;

namespace Antiaim
{
	void Run(CUserCmd* cmd, FakelagState state);
}

#endif
