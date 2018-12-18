#ifdef HANDLE_OPTION
//Syntax: HANDLE_OPTION(type, defaultValue, name, settings group chain...)
	HANDLE_OPTION(bool, true, bunnyhopping, bindSettings, globalSettings)
	HANDLE_OPTION(bool, true, autostrafer, bindSettings, globalSettings)
	HANDLE_OPTION(bool, true, antiaim, bindSettings, globalSettings)

	HANDLE_OPTION(bool, true, fakelag, bindSettings, globalSettings)

	HANDLE_OPTION(bool, true, aimbot, bindSettings, globalSettings)
	HANDLE_OPTION(bool, true,aimbotSetAngles, bindSettings, globalSettings)
	HANDLE_OPTION(int, 1, aimbotLagCompensation, bindSettings, globalSettings)
	HANDLE_OPTION(int, 0, aimbotHitChance, bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, aimbotAutoShoot, bindSettings, globalSettings)
	HANDLE_OPTION(bool, true, aimbotBacktrack, bindSettings, globalSettings)
	HANDLE_OPTION(bool, true, aimbotSafeBacktrack, bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, aimbotNospread, bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, resolver, bindSettings, globalSettings)
#undef HANDLE_OPTION
#else
static_assert(false, "HANDLE_OPTION not defined!");
#endif
