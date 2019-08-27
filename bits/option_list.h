#ifdef HANDLE_OPTION
//Syntax: HANDLE_OPTION(type, defaultValue, name, description, settings group chain...)
	HANDLE_OPTION(bool, false, showMenu, "Show Menu", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, bunnyhopping, "Bunny Hopping", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, autostrafer, "Automatically strafe and achieve high velocity", bindSettings, globalSettings)
	HANDLE_OPTION(float, 1.3f, autostraferControl, "The control camera movement has to the strafer. Higher values allow for sharper turns, but result higher loss of velocity", bindSettings, globalSettings)
#ifdef TESTING_FEATURES
	HANDLE_OPTION(bool, false, antiaim, "Desync player animations", bindSettings, globalSettings)

	HANDLE_OPTION(int, 0, fakelag, "Choke packets (count)", bindSettings, globalSettings)
	HANDLE_OPTION(int, 0, fakelagBreakLC, "Break lag compensation while in air", bindSettings, globalSettings)
#endif

	HANDLE_OPTION(bool, false, aimbot, "Aim at enemy players", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, aimbotSetAngles, "Set (silent) angles at the best aimbot target", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, aimbotSetViewAngles, "Set camera angles when running aimbot", bindSettings, globalSettings)
	HANDLE_OPTION(int, 20, aimbotMinDamage, "The minimum damage aimbot tries to achieve", bindSettings, globalSettings)
	HANDLE_OPTION(float, 360, aimbotFOV, "The effective range around the camera in which the aimbot will try to find targets.", bindSettings, globalSettings)
#ifdef TESTING_FEATURES
	HANDLE_OPTION(int, 1, aimbotLagCompensation, "Predict enemy players into the future if necessary to hit (1 is low overhead, 2 is full)", bindSettings, globalSettings)
#endif
	HANDLE_OPTION(int, 0, aimbotHitChance, "Do not shoot if the chance to hit a player is below certain threshold", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, aimbotAutoShoot, "Automatically shoot when the target was found", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, aimbotBacktrack, "Try to move players back in time so they end up on the crosshair or somewhere aimable", bindSettings, globalSettings)
#ifdef TESTING_FEATURES
	HANDLE_OPTION(bool, false, aimbotSafeBacktrack, "Do not to set time lower than it was during previous tick (WIP, not functional)", bindSettings, globalSettings)
#endif
	HANDLE_OPTION(bool, false, aimbotNospread, "Compensate weapon inaccuracy (works only on servers supporting this feature)", bindSettings, globalSettings)
	HANDLE_OPTION(int, 400, traceBudget, "The budget of game tracing (used in aimbot). Lower values yield better performance, yet higher values check more spots when running aimbot", bindSettings, globalSettings)
#ifdef TESTING_FEATURES
	HANDLE_OPTION(bool, false, resolver, "Try to resolve players desyncing animations", bindSettings, globalSettings)
	HANDLE_OPTION(int, 0, debugVisuals, "Debug visuals (0 off, 1 low overhead, 2 all on)", bindSettings, globalSettings)
#ifdef MTR_ENABLED
	HANDLE_OPTION(bool, false, perfTrace, "Performance profile and save the result to a file (bind this to key)", bindSettings, globalSettings)
#endif
#endif
	HANDLE_OPTION(bool, false, rageMode, "Enable rage mode.", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, thirdPerson, "Enable third person", bindSettings, globalSettings)
#ifdef TESTING_FEATURES
	HANDLE_OPTION(bool, false, headCam, "Draw camera in head position", bindSettings, globalSettings)
#endif
	HANDLE_OPTION(bool, false, thirdPersonShowReal, "Show real angle in third person instead of fake", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, disablePostProcessing, "Disable game's post processing effects", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, noFlash, "Disable flashbang effect", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, noSmoke, "Disable smoke effect", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, noFog, "Disable in-game fog", bindSettings, globalSettings)
#undef HANDLE_OPTION
#else
static_assert(false, "HANDLE_OPTION not defined!");
#endif
