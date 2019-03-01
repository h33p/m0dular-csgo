#include "gamemenu.h"
#include "menu/menu.h"
#include "menu/menuimpl.h"
#include "../core/settings.h"
#include "../core/binds.h"

void RageBotTab();
void VisualsTab();
void AntiaimTab();
void MiscTab();
void OtherTab();

//It should be different depending on whether or not rage mode is activated
static constexpr MenuTab tabsRage[] = {
	{"Aimbot", RageBotTab},
	{"Visuals", VisualsTab},
	{"Antiaim", AntiaimTab},
	{"Misc", MiscTab},
	{"Other", OtherTab}
};

void MenuImpl::Render()
{
	Menu::Render(tabsRage, sizeof(tabsRage) / sizeof(tabsRage[0]));
}

void RageBotTab()
{

}

void VisualsTab()
{

}

void AntiaimTab()
{

}

void MiscTab()
{

}

void OtherTab()
{

}
