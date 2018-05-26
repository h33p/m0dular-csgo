#ifndef HOOKS_H
#define HOOKS_H

#include "../framework/utils/vfhook.h"
#include "../framework/source_csgo/sdk.h"
#include "../framework/source_shared/hooks.h"

#define GetOriginal1(NAME) GetOriginal<decltype(NAME)*>((void*)NAME);
#define GetOriginal2(TYPE, NAME) GetOriginal<TYPE>((void*)NAME);

#define ID2(x) x
#define GET_MACRO2(_1,_2, NAME,...) NAME
#define GetOriginal(...) ID2(GET_MACRO2(__VA_ARGS__, GetOriginal2, GetOriginal1)(__VA_ARGS__))

extern VFuncHook* hookClientMode;
extern VFuncHook* hookCl;
extern VFuncHook* hookEngine;

#endif
