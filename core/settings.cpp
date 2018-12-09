#include "settings.h"
#include "../sdk/framework/features/aimbot.h"

namespace Settings
{
	SettingsGroup globalSettings;
	SettingsGroup bindSettings;
	AimbotHitbox aimbotHitboxes[MAX_HITBOXES] = {
		{ HITBOX_HEAD, SCAN_MULTIPOINT, 0.8f },
		{ HITBOX_NECK, SCAN_SIMPLE },
		{ HITBOX_PELVIS, SCAN_SIMPLE },
		{ HITBOX_STOMACH, SCAN_SIMPLE },
		{ HITBOX_LOWER_CHEST, SCAN_SIMPLE },
		{ HITBOX_CHEST, SCAN_MULTIPOINT, 0.8f },
		{ HITBOX_RIGHT_THIGH, SCAN_SIMPLE },
		{ HITBOX_LEFT_THIGH, SCAN_SIMPLE },
		{ HITBOX_RIGHT_CALF, SCAN_SIMPLE },
		{ HITBOX_LEFT_CALF, SCAN_SIMPLE },
		{ HITBOX_RIGHT_FOOT, SCAN_MULTIPOINT, 0.8f },
		{ HITBOX_LEFT_FOOT, SCAN_MULTIPOINT, 0.8f },
		{ HITBOX_RIGHT_HAND, SCAN_MULTIPOINT, 0.8f },
		{ HITBOX_LEFT_HAND, SCAN_MULTIPOINT, 0.8f },
		{ HITBOX_RIGHT_UPPER_ARM, SCAN_SIMPLE },
		{ HITBOX_LEFT_UPPER_ARM, SCAN_SIMPLE },
	};

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
	OPTIONDEF(aimbotAutoShoot)(false);
	OPTIONDEF(aimbotNospread)(false);
}
