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
	{hookClientMode, (uintptr_t*)&SourceHooks::CreateMove, 25}
};
#elif defined(__APPLE__)
HookDefine hookIds[] = {
	{hookClientMode, (uintptr_t*)&SourceHooks::CreateMove, 25}
};
#elif defined(_WIN32) || defined(_WIN64)
HookDefine hookIds[] = {
	{hookClientMode, (uintptr_t*)&SourceHooks::CreateMove, 24}
};
#endif

#endif
