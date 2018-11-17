#include "settings.h"

namespace Settings
{
	SettingsGroup globalSettings;
	SettingsGroup bindSettings;

	OPTION(bool, bunnyhopping, bindSettings, globalSettings)(true);
	OPTION(bool, autostrafer, bindSettings, globalSettings)(true);
	OPTION(bool, antiaim, bindSettings, globalSettings)(true);

	OPTION(bool, fakelag, bindSettings, globalSettings)(true);

	OPTION(bool, aimbot, bindSettings, globalSettings)(true);
	OPTION(bool, aimbotSetAngles, bindSettings, globalSettings)(true);
	OPTION(int, aimbotLagCompensation, bindSettings, globalSettings)(true);
	OPTION(int, aimbotHitChance, bindSettings, globalSettings)(0);
	OPTION(bool, aimbotSafeBacktrack, bindSettings, globalSettings)(false);
	OPTION(bool, aimbotBacktrack, bindSettings, globalSettings)(true);
	OPTION(bool, aimbotAutoShoot, bindSettings, globalSettings)(false);
}
