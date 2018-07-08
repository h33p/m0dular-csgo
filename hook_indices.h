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
#ifdef PT_VISUALS
	{hookPanel, (uintptr_t*)&CSGOHooks::PaintTraverse, 42}
#endif
};
#elif defined(__APPLE__)
HookDefine hookIds[] = {
	{hookClientMode, (uintptr_t*)&SourceHooks::CreateMove, 25},
#ifdef PT_VISUALS
	{hookPanel, (uintptr_t*)&CSGOHooks::PaintTraverse, 42}
#endif
};
#elif defined(_WIN32) || defined(_WIN64)
HookDefine hookIds[] = {
	{hookClientMode, (uintptr_t*)&SourceHooks::CreateMove, 24},
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
	{CSGOHooks::VecAnglesProxy, CCRC32("DT_BaseEntity"), CCRC32("m_angRotation[1]"), nullptr}
};

size_t netvarCount = sizeof(netvarHooks) / sizeof(NetvarHook);

#endif
