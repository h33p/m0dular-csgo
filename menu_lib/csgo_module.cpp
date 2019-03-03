#include "settings.h"
#include "../core/settings.h"
#include "../core/binds.h"
#include "../core/shmfs.h"
#include "../pch/menufont.h"

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

#if defined(__linux__) || defined(__APPLE__)
#define APIENTRY __attribute__((constructor))
#endif

int APIENTRY DllMain(void* hModule, uintptr_t reasonForCall, void* lpReserved)
{
#ifdef _WIN32
	if (reasonForCall == DLL_PROCESS_ATTACH)
#endif
		SHMFS::sharedInstance->SetEntry("MenuFont"_crc32, menuFont_compressed_data_base85, sizeof(menuFont_compressed_data_base85));
	return 1;
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
