#ifndef HOOKS_H
#define HOOKS_H

#include "../sdk/framework/utils/vfhook.h"
#include "../sdk/framework/utils/atomic_lock.h"
#include "../sdk/source_csgo/sdk.h"
#include "../sdk/source_shared/hooks.h"
#include <unordered_map>

#define GetOriginal1(NAME) GetOriginal<decltype(NAME)*>((void*)NAME);
#define GetOriginal2(TYPE, NAME) GetOriginal<TYPE>((void*)NAME);

#define ID2(x) x
#define GET_MACRO2(_1,_2, NAME,...) NAME
#define GetOriginal(...) ID2(GET_MACRO2(__VA_ARGS__, GetOriginal2, GetOriginal1)(__VA_ARGS__))

extern VFuncHook* hookClientMode;

namespace CSGOHooks
{
	void __fastcall OverrideView(FASTARGS, CViewSetup*);
	int __fastcall DoPostScreenSpaceEffects(FASTARGS, CViewSetup*);
	void DidSmokeEffectProxy(const CRecvProxyData* data, void* ent, void* out);
}

#endif
