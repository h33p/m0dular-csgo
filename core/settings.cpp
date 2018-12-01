#include "settings.h"

namespace Settings
{
	SettingsGroup globalSettings;
	SettingsGroup bindSettings;

	OPTIONDEF(bunnyhopping)(true);
	OPTIONDEF(autostrafer)(true);
	OPTIONDEF(antiaim)(true);

	OPTIONDEF(fakelag)(true);

	OPTIONDEF(aimbot)(true);
	OPTIONDEF(aimbotSetAngles)(true);
	OPTIONDEF(aimbotLagCompensation)(true);
	OPTIONDEF(aimbotHitChance)(0);
	OPTIONDEF(aimbotSafeBacktrack)(false);
	OPTIONDEF(aimbotBacktrack)(true);
	OPTIONDEF(aimbotAutoShoot)(true);
}
