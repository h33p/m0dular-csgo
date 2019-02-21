#include "settings.h"
#include "../core/settings.h"
#include "../core/binds.h"

static uintptr_t clAllocBase = 0;
static generic_free_list_allocator<clAllocBase, true> clAlloc(10000, PlacementPolicy::FIND_FIRST);

static int slid = 0;

static ConsoleSetting csgoSettingList[Settings::optionCount] =
{
#define HANDLE_OPTION(type, defaultVal, name, description, ...) {slid++, CCRC32(#name), STALLOC(#name), STALLOC(description), CCRC32(#type), &Settings::name, &GetPrintSetting<type, decltype(Settings::name)::Get> },
#include "../bits/option_list.h"
};

void OnLoad(ConsoleSetting** sets, size_t* size)
{
	*sets = csgoSettingList;
	*size = Settings::optionCount;
}

extern "C"
{
#ifdef _WIN32
	__declspec(dllexport)
#else
	__attribute__ ((visibility ("default")))
#endif
		void Menu()
	{
		SettingsConsole();
	}
}
