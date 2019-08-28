#include "gamemenu.h"
#include "menu/menu.h"
#include "menu/menuimpl.h"
#include "menu/widgets.h"
#include "../core/settings.h"
#include "../core/binds.h"

//Defined in init.cpp
void Unload();

void LegitBotTab();
void RageBotTab();
void VisualsTab();
void AntiaimTab();
void MiscTab();
void OtherTab();

static constexpr MenuTab tabsRage[] = {
	{"Aimbot", RageBotTab},
	{"Visuals", VisualsTab},
	{"Antiaim", AntiaimTab},
	{"Misc", MiscTab},
	{"Other", OtherTab}
};

static constexpr MenuTab tabsLegit[] = {
	{"Aimbot", LegitBotTab},
	{"Visuals", VisualsTab},
	{"Antiaim", AntiaimTab},
	{"Misc", MiscTab},
	{"Other", OtherTab}
};

static std::unique_ptr<SettingsGroupAccessorBase> activeGroup;

template<typename T, crcs_t CRC>
struct OptionWrap
{
	bool exists;
	bool modified;
	T val;

	OptionWrap()
		: exists(activeGroup->IsAlloced(CRC)), modified(false)
	{
		if (exists)
			val = activeGroup->Get<T>(CRC);
		else
			val = T();
	}

	~OptionWrap()
	{
		if (modified)
			activeGroup->Set(CRC, val);
	}

	constexpr operator T()
	{
		return val;
	}

	constexpr auto& operator = (const T& v)
	{
		if (val != v)
			modified = true;
		val = v;
		return *this;
	}
};

template<typename T>
static void SettingsCheckBox(const T& val, const char* str)
{
	constexpr crcs_t CRC = T::CRCVAL;
	OptionWrap<T::value_type, CRC> wrapper;
	CheckBox::Run(wrapper, str);
}

template<typename T>
static void SettingsSlider(const T& val, typename T::value_type start, typename T::value_type end, const char* str)
{
	constexpr crcs_t CRC = T::CRCVAL;
	OptionWrap<T::value_type, CRC> wrapper;
	Slider<T::value_type>::Run(wrapper, start, end, str);
}

void MenuImpl::Render()
{
	const MenuTab* tabs = tabsRage;
	size_t tabsSize = sizeof(tabsRage) / sizeof(MenuTab);

	if (!activeGroup)
		activeGroup.reset(Settings::globalSettings->GenerateNewAccessor());

	if (!Settings::rageMode) {
		tabs = tabsLegit;
		tabsSize = sizeof(tabsLegit) / sizeof(tabsLegit[0]);
	}

	Menu::Render(tabs, tabsSize);
}

void LegitBotTab()
{
	TabPad pad;
	IdPush id(tabsLegit);

	ImGui::Columns(2, nullptr, false);

	SettingsCheckBox(Settings::aimbot, "Aimbot");

	SettingsCheckBox(Settings::aimbotSetAngles, "Aim (silent)");
	SettingsCheckBox(Settings::aimbotSetViewAngles, "Aim (non-silent)");
	SettingsCheckBox(Settings::aimbotBacktrack, "Backtrack");

#ifdef TESTING_FEATURES
	SettingsCheckBox(Settings::aimbotSafeBacktrack, "Safe backtrack");
#endif

	SettingsSlider(Settings::aimbotMinDamage, 0, 100, "Minimum damage");
	SettingsSlider(Settings::aimbotFOV, 0, 360, "Aim FOV");
	SettingsSlider(Settings::aimbotHitChance, 0, 100, "HitChance");
}

void RageBotTab()
{
	TabPad pad;
	IdPush id(tabsRage);

	ImGui::Columns(2, nullptr, false);

	SettingsCheckBox(Settings::aimbot, "Aimbot");

	SettingsCheckBox(Settings::aimbotSetAngles, "Aim (silent)");
	SettingsCheckBox(Settings::aimbotSetViewAngles, "Aim (non-silent)");
	SettingsCheckBox(Settings::aimbotBacktrack, "Backtrack");

	SettingsSlider(Settings::aimbotMinDamage, 0, 100, "Minimum damage");
	SettingsSlider(Settings::aimbotFOV, 0, 360, "Aim FOV");
	SettingsSlider(Settings::aimbotHitChance, 0, 100, "HitChance");

	SettingsCheckBox(Settings::aimbotNospread, "Compensate spread");
#ifdef TESTING_FEATURES
	SettingsCheckBox(Settings::resolver, "Enable resolver");
#endif
}

void VisualsTab()
{
	TabPad pad;
	IdPush id(tabsLegit);

	ImGui::Columns(2, nullptr, false);

	SettingsCheckBox(Settings::noFlash, "NoFlash");
	SettingsCheckBox(Settings::noSmoke, "NoSmoke");
	SettingsCheckBox(Settings::noFog, "NoFog");
	SettingsCheckBox(Settings::disablePostProcessing, "NoPostProcess");

	SettingsCheckBox(Settings::thirdPerson, "Third Person");
#ifdef TESTING_FEATURES
	SettingsCheckBox(Settings::headCam, "Head Camera");

	SettingsCheckBox(Settings::debugVisuals, "Debug visuals");
#endif
}

void AntiaimTab()
{
	TabPad pad;
	IdPush id(tabsLegit);

	ImGui::Columns(2, nullptr, false);

#ifdef TESTING_FEATURES
	SettingsCheckBox(Settings::antiaim, "AntiAim");
	SettingsSlider(Settings::fakelag, 0, 14, "FakeLag");
	SettingsCheckBox(Settings::fakelagBreakLC, "FakeLag");
#endif
}

void MiscTab()
{
	TabPad pad;
	IdPush id(tabsLegit);

	ImGui::Columns(2, nullptr, false);

	SettingsCheckBox(Settings::bunnyhopping, "Bunnyhopping");
	SettingsCheckBox(Settings::autostrafer, "Autostrafer");
	SettingsSlider(Settings::autostraferControl, 0, 2, "Strafe control");
}

void OtherTab()
{
	TabPad pad;
	IdPush id(tabsLegit);

	ImGui::Columns(2, nullptr, false);

	if (ImGui::Button("Unload"))
		Unload();

	SettingsCheckBox(Settings::rageMode, "Rage Mode");
	SettingsSlider(Settings::traceBudget, 10, 1000, "Trace Budget");

#ifdef TESTING_FEATURES
	SettingsCheckBox(Settings::perfTrace, "Performance Profiling");
#endif
}
