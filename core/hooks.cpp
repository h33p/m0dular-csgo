#include "../framework/g_defines.h"
#include "../framework/source_shared/hooks.h"
#include "hooks.h"

void __fastcall SourceHooks::CreateMove(FASTARGS, float inputSampleTime, CUserCmd* cmd)
{
	static auto origFn = hookClientMode->GetOriginal(SourceHooks::CreateMove);
}
