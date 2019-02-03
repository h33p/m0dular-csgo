#ifndef CLIENT_SETTINGS_H
#define CLIENT_SETTINGS_H

#include <iostream>
#include "../sdk/framework/utils/crc32.h"
#include "../core/settings.h"
#include "../core/binds.h"

typedef void(*PrintFn)(void*);
typedef void(*PrintGroupFn)(crcs_t);

struct ConsoleSetting
{
	const int id;
	const crcs_t crc;
	const char* name;
	const char* description;
	const crcs_t typeNameCRC;
	void* settingPtr;
	const PrintFn printFunction;
};

#define STTYPE(name) decltype(StackString(name))
#define STALLOC(name) *(new (clAlloc.allocate<STTYPE(name)>(1)) StackString(name))

template<auto& Group, typename T>
T GetSetting(crcs_t crc)
{
    return Group->template GetRuntime<T>(crc);
}

template<typename T, auto& Fn>
void GetPrintSetting(void* in)
{
	typedef T(*FnVFn)(void*);
	std::cout << ((FnVFn)Fn)(in);
}

template<typename T, auto& Fn>
void GetPrintSettingGroups(crcs_t crc)
{
	std::cout << GetSetting<Settings::globalSettings, T>(crc) << " " << GetSetting<Settings::bindSettings, T>(crc);
}

template<auto& Group, typename T>
void SetSetting(crcs_t crc, T value)
{
    Group->SetRuntime(value, crc);
}

void SettingsConsole();

#endif
