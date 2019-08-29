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
	{hookClientMode, (uintptr_t*)&CSGOHooks::OverrideView, 19},
	{hookClientMode, (uintptr_t*)&CSGOHooks::DoPostScreenSpaceEffects, 45},
	{hookViewRender, (uintptr_t*)&CSGOHooks::OnRenderStart, 4},
	{hookSurface, (uintptr_t*)&CSGOHooks::LockCursor, 68},
#ifdef PT_VISUALS
	{hookPanel, (uintptr_t*)&CSGOHooks::PaintTraverse, 42}
#endif
};
#elif defined(__APPLE__)
HookDefine hookIds[] = {
	{hookClientMode, (uintptr_t*)&SourceHooks::CreateMove, 25},
	{hookClientMode, (uintptr_t*)&CSGOHooks::OverrideView, 19},
	{hookClientMode, (uintptr_t*)&CSGOHooks::DoPostScreenSpaceEffects, 45},
	{hookViewRender, (uintptr_t*)&CSGOHooks::OnRenderStart, 4},
	{hookSurface, (uintptr_t*)&CSGOHooks::LockCursor, 68},
#ifdef PT_VISUALS
	{hookPanel, (uintptr_t*)&CSGOHooks::PaintTraverse, 42}
#endif
};
#elif defined(_WIN32) || defined(_WIN64)
HookDefine hookIds[] = {
	{hookClientMode, (uintptr_t*)&SourceHooks::CreateMove, 24},
	{hookClientMode, (uintptr_t*)&CSGOHooks::OverrideView, 18},
	{hookClientMode, (uintptr_t*)&CSGOHooks::DoPostScreenSpaceEffects, 44},
	{hookViewRender, (uintptr_t*)&CSGOHooks::OnRenderStart, 4},
	{hookSurface, (uintptr_t*)&CSGOHooks::LockCursor, 67},
	{hookD3D, (uintptr_t*)&PlatformHooks::Present, 17},
	{hookD3D, (uintptr_t*)&PlatformHooks::Reset, 16},
#ifdef PT_VISUALS
	{hookPanel, (uintptr_t*)&CSGOHooks::PaintTraverse, 41}
#endif
};
#endif

EffectHook effectHooks[] = {
	{CSGOHooks::ImpactsEffect, CCRC32("Impact"), nullptr}
};

size_t effectsCount = sizeof(effectHooks) / sizeof(EffectHook);

NetvarHook netvarHooks[] = {
	{CSGOHooks::LBYProxy, CCRC32("DT_CSPlayer"), CCRC32("m_flLowerBodyYawTarget"), nullptr},
	{CSGOHooks::DidSmokeEffectProxy, CCRC32("DT_SmokeGrenadeProjectile"), CCRC32("m_bDidSmokeEffect"), nullptr},
};

size_t netvarCount = sizeof(netvarHooks) / sizeof(NetvarHook);

#endif
