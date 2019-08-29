#include "gamemenu.h"
#include "menu/menu.h"
#include "menu/menuimpl.h"
#include "menu/widgets.h"
#include "menu/uielements.h"
#include "../core/settings.h"
#include "../core/binds.h"

//Defined in init.cpp
void Unload();

void RenderTab(void*);

static void RenderUnusedElements();
template<typename T>
static void SettingsCheckBox(const T& val, const char* str, const char* tooltip = nullptr);
template<typename T>
static void SettingsSlider(const T& val, typename T::value_type start, typename T::value_type end, const char* str, const char* tooltip = nullptr);
template<typename T, typename F>
static void SettingsVecSlider(const T& val, F start, F end, const char* str, const char* tooltip = nullptr);

template<typename T, crcs_t CRC>
struct OptionWrap;

UIElement allElements[] = {
#define HANDLE_OPTION(type, defaultVal, minVal, maxVal, name, uiName, description, ...) { CCRC32(#name), 0, [](UIElement*) {if constexpr (std::is_same<type, bool>::value) SettingsCheckBox(Settings:: name, ST(uiName), ST(description)); else if constexpr (std::is_arithmetic<type>::value) SettingsSlider(Settings:: name, minVal, maxVal, ST(uiName), ST(description)); else SettingsVecSlider(Settings::name, minVal, maxVal, ST(uiName), ST(description)); }},
#include "../bits/option_list.h"
	{ 0, 0, [](UIElement*) { RenderUnusedElements(); } },
	{ 1, 0, [](UIElement*) { if (ImGui::Button(ST("Unload"))) Unload(); } },
};

std::vector<UIColumn> legitBotColumns = {
	UIColumn(Settings::aimbot
		, Settings::aimbotSetAngles
		, Settings::aimbotSetViewAngles
		, Settings::aimbotBacktrack
#ifdef TESTING_FEATURES
		, Settings::aimbotSafeBacktrack
#endif
		, Settings::aimbotMinDamage
		, Settings::aimbotFOV
		, Settings::aimbotHitChance
		),
	UIColumn()
};

std::vector<UIColumn> rageBotColumns = {
	UIColumn(Settings::aimbot
		, Settings::aimbotSetAngles
		, Settings::aimbotSetViewAngles
		, Settings::aimbotBacktrack
		, Settings::aimbotAutoShoot
		, Settings::aimbotMinDamage
		, Settings::aimbotFOV
		, Settings::aimbotHitChance
		, Settings::aimbotNospread
#ifdef TESTING_FEATURES
		, Settings::aimbotLagCompensation
		, Settings::resolver
#endif
		),
	UIColumn()
};

std::vector<UIColumn> visualsColumns = {
	UIColumn(Settings::noFlash
		, Settings::noSmoke
		, Settings::noFog
		, Settings::disablePostProcessing
		, Settings::thirdPerson
#ifdef TESTING_FEATURES
		, Settings::headCam
		, Settings::debugVisuals
#endif
		),
	UIColumn(Settings::glow
		, Settings::glowOutline
		, Settings::glowEnemy
		, Settings::glowEnemyColor
		, Settings::glowTeam
		, Settings::glowTeamColor
		, Settings::glowWeapons
		, Settings::glowWeaponsColor
		, Settings::glowC4
		, Settings::glowC4Color
		)
};

std::vector<UIColumn> antiaimColumns = {
	UIColumn(
#ifdef TESTING_FEATURES
		Settings::antiaim
		, Settings::fakelag
		, Settings::fakelagBreakLC
#endif
		),
	UIColumn()
};

std::vector<UIColumn> miscColumns = {
	UIColumn(Settings::bunnyhopping
		, Settings::autostrafer
		, Settings::autostraferControl
		),
	UIColumn()
};

std::vector<UIColumn> otherColumns = {
	UIColumn(crcs_t(1)
		, Settings::rageMode
		, Settings::traceBudget
#ifdef TESTING_FEATURES
		, Settings::perfTrace
#endif
		),
	UIColumn(crcs_t(0))
};

static constexpr MenuTab tabsRage[] = {
	{"Aimbot", RenderTab, &rageBotColumns},
	{"Visuals", RenderTab, &visualsColumns},
	{"Antiaim", RenderTab, &antiaimColumns},
	{"Misc", RenderTab, &miscColumns},
	{"Other", RenderTab, &otherColumns}
};

static constexpr MenuTab tabsLegit[] = {
	{"Aimbot", RenderTab, &legitBotColumns},
	{"Visuals", RenderTab, &visualsColumns},
	{"Antiaim", RenderTab, &antiaimColumns},
	{"Misc", RenderTab, &miscColumns},
	{"Other", RenderTab, &otherColumns}
};

static std::unique_ptr<SettingsGroupAccessorBase> activeGroup;

void MenuImpl::Render()
{
	const MenuTab* tabs = tabsRage;
	size_t tabsSize = sizeof(tabsRage) / sizeof(MenuTab);

	UIElements::InitializeColumns(std::begin(legitBotColumns), std::end(legitBotColumns), std::begin(allElements), std::end(allElements));
	UIElements::InitializeColumns(std::begin(rageBotColumns), std::end(rageBotColumns), std::begin(allElements), std::end(allElements));
	UIElements::InitializeColumns(std::begin(visualsColumns), std::end(visualsColumns), std::begin(allElements), std::end(allElements));
	UIElements::InitializeColumns(std::begin(antiaimColumns), std::end(antiaimColumns), std::begin(allElements), std::end(allElements));
	UIElements::InitializeColumns(std::begin(miscColumns), std::end(miscColumns), std::begin(allElements), std::end(allElements));
	UIElements::InitializeColumns(std::begin(otherColumns), std::end(otherColumns), std::begin(allElements), std::end(allElements));

	if (!activeGroup)
		activeGroup.reset(Settings::globalSettings->GenerateNewAccessor());

	if (!Settings::rageMode) {
		tabs = tabsLegit;
		tabsSize = sizeof(tabsLegit) / sizeof(tabsLegit[0]);
	}

	Menu::Render(tabs, tabsSize);
}

void RenderTab(void* passData)
{
	TabPad pad;
	IdPush id(passData);
	std::vector<UIColumn>* columns = (std::vector<UIColumn>*)passData;
	UIElements::RenderColumns(columns->begin(), columns->end());
}


template<typename T, crcs_t CRC>
struct OptionWrap
{
	bool exists;
	T val;
	T initVal;

	OptionWrap()
		: exists(activeGroup->IsAlloced(CRC))
	{
		if (exists)
			val = activeGroup->Get<T>(CRC);
		else
			val = T();

		initVal = val;
	}

	~OptionWrap()
	{
		if (val != initVal)
			activeGroup->Set(CRC, val);
	}

	constexpr operator T()
	{
		return val;
	}

	constexpr auto& operator = (const T& v)
	{
		val = v;
		return *this;
	}

	constexpr auto& operator[] (size_t i)
	{
		return val[i];
	}
};

template<typename T>
static void SettingsCheckBox(const T& val, const char* str, const char* tooltip)
{
	constexpr crcs_t CRC = T::CRCVAL;
	OptionWrap<typename T::value_type, CRC> wrapper;
	CheckBox::Run(wrapper, str, tooltip);
}

template<typename T>
static void SettingsSlider(const T& val, typename T::value_type start, typename T::value_type end, const char* str, const char* tooltip)
{
	constexpr crcs_t CRC = T::CRCVAL;
	OptionWrap<typename T::value_type, CRC> wrapper;
	Slider<typename T::value_type>::Run(wrapper, start, end, str, tooltip);
}

template<typename T, typename F>
static void SettingsVecSlider(const T& val, F start, typename F end, const char* str, const char* tooltip)
{
	constexpr crcs_t CRC = T::CRCVAL;
	OptionWrap<typename T::value_type, CRC> wrapper;
	SliderVec<typename T::value_type::value_type, T::value_type::Yt>::Run(wrapper, start, end, str, tooltip);
}

static void RenderUnusedElements()
{
	bool firstTime = true;

	for (UIElement& el : allElements) {
		if (el.hash > 10 && !el.refCount) {
			if (firstTime)
				ImGui::LabelText(ST("Unassigned options:"), "");
			firstTime = false;
			el.Render();
		}
	}
}
