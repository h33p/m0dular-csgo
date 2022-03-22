#ifndef HOOK_INDICES_H
#define HOOK_INDICES_H

typedef struct
{
	VFuncHook*& hook;
	uintptr_t* function;
	uintptr_t index;
} HookDefine;

#if defined(__linux__)
HookDefine hookIds[] = {
	{hookClientMode, (uintptr_t*)&SourceHooks::CreateMove, 25},
	{hookClientMode, (uintptr_t*)&CSGOHooks::DoPostScreenSpaceEffects, 45},
	{hookClientMode, (uintptr_t*)&CSGOHooks::OverrideView, 19},
};
#elif defined(__APPLE__)
HookDefine hookIds[] = {
	{hookClientMode, (uintptr_t*)&SourceHooks::CreateMove, 25},
	{hookClientMode, (uintptr_t*)&CSGOHooks::DoPostScreenSpaceEffects, 45},
	{hookClientMode, (uintptr_t*)&CSGOHooks::OverrideView, 19},
};
#elif defined(_WIN32) || defined(_WIN64)
HookDefine hookIds[] = {
	{hookClientMode, (uintptr_t*)&SourceHooks::CreateMove, 24},
	{hookClientMode, (uintptr_t*)&CSGOHooks::DoPostScreenSpaceEffects, 44},
	{hookClientMode, (uintptr_t*)&CSGOHooks::OverrideView, 18},
};
#endif

NetvarHook netvarHooks[] = {
	{CSGOHooks::DidSmokeEffectProxy, CCRC32("DT_SmokeGrenadeProjectile"), CCRC32("m_bDidSmokeEffect"), nullptr},
};

size_t netvarCount = sizeof(netvarHooks) / sizeof(NetvarHook);

#endif
