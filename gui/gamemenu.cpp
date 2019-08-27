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

void MenuImpl::Render()
{
	const MenuTab* tabs = tabsRage;
	size_t tabsSize = sizeof(tabsRage) / sizeof(MenuTab);

	if (!Settings::rageMode) {
		tabs = tabsLegit;
		tabsSize = sizeof(tabsLegit) / sizeof(tabsLegit[0]);
	}

	Menu::Render(tabs, tabsSize);
}

void LegitBotTab()
{
	TabPad pad;

	CheckBox::Run(Settings::aimbot, "Aimbot");

	CheckBox::Run(Settings::aimbotSetAngles, "Aim (silent)");
	CheckBox::Run(Settings::aimbotSetViewAngles, "Aim (non-silent)");
	CheckBox::Run(Settings::aimbotBacktrack, "Backtrack");

#ifdef TESTING_FEATURES
	CheckBox::Run(Settings::aimbotBacktrack, "Safe backtrack");
#endif

	Slider<int>::Run(Settings::aimbotMinDamage, 0, 100, "Minimum damage");
	Slider<float>::Run(Settings::aimbotFOV, 0, 360, "Aim FOV");
	Slider<int>::Run(Settings::aimbotHitChance, 0, 100, "HitChance");
}

void RageBotTab()
{
	TabPad pad;

	CheckBox::Run(Settings::aimbot, "Aimbot");

	CheckBox::Run(Settings::aimbotSetAngles, "Aim (silent)");
	CheckBox::Run(Settings::aimbotSetViewAngles, "Aim (non-silent)");
	CheckBox::Run(Settings::aimbotBacktrack, "Backtrack");

	Slider<int>::Run(Settings::aimbotMinDamage, 0, 100, "Minimum damage");
	Slider<float>::Run(Settings::aimbotFOV, 0, 360, "Aim FOV");
	Slider<int>::Run(Settings::aimbotHitChance, 0, 100, "HitChance");

	CheckBox::Run(Settings::aimbotNospread, "Compensate spread");
#ifdef TESTING_FEATURES
	CheckBox::Run(Settings::resolver, "Enable resolver");
#endif
}

void VisualsTab()
{
	TabPad pad;

	CheckBox::Run(Settings::noFlash, "NoFlash");
	CheckBox::Run(Settings::noSmoke, "NoSmoke");
	CheckBox::Run(Settings::noFog, "NoFog");
	CheckBox::Run(Settings::disablePostProcessing, "NoPostProcess");

	CheckBox::Run(Settings::thirdPerson, "Third Person");
#ifdef TESTING_FEATURES
	CheckBox::Run(Settings::headCam, "Head Camera");

	CheckBox::Run(Settings::debugVisuals, "Debug visuals");
#endif
}

void AntiaimTab()
{
	TabPad pad;

#ifdef TESTING_FEATURES
	CheckBox::Run(Settings::antiaim, "AntiAim");
	Slider<int>::Run(Settings::fakelag, 0, 14, "FakeLag");
	CheckBox::Run(Settings::fakelagBreakLC, "FakeLag");
#endif
}

void MiscTab()
{
	TabPad pad;

	CheckBox::Run(Settings::bunnyhopping, "Bunnyhopping");
	CheckBox::Run(Settings::autostrafer, "Autostrafer");
	Slider<float>::Run(Settings::autostraferControl, 0, 2, "Strafe control");
}

void OtherTab()
{
	TabPad pad;

	if (ImGui::Button("Unload"))
		Unload();

	CheckBox::Run(Settings::rageMode, "Rage Mode");
	Slider<int>::Run(Settings::traceBudget, 10, 1000, "Trace Budget");

#ifdef TESTING_FEATURES
	CheckBox::Run(Settings::perfTrace, "Performance Profiling");
#endif
}
