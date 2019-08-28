#ifdef HANDLE_OPTION
//Syntax: HANDLE_OPTION(type, defaultValue, minValue, maxValue, name, UI name, description, settings group chain...)
	HANDLE_OPTION(bool, false, false, true, showMenu, "Show Menu", "Show Menu", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, false, true, bunnyhopping, "Bunnyhopping", "Bunny Hopping", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, false, true, autostrafer, "Autostrafer", "Automatically strafe and achieve high velocity", bindSettings, globalSettings)
	HANDLE_OPTION(float, 1.3f, 0.f, 2.f, autostraferControl, "Autostrafer Control", "The control camera movement has to the strafer. Higher values allow for sharper turns, but result higher loss of velocity", bindSettings, globalSettings)
#ifdef TESTING_FEATURES
	HANDLE_OPTION(bool, false, false, true, antiaim, "AntiAim", "Desync player animations", bindSettings, globalSettings)

	HANDLE_OPTION(int, 0, 0, 14, fakelag, "FakeLag", "Choke packets (count)", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, false, true, fakelagBreakLC, "Break LC", "Break lag compensation while in air", bindSettings, globalSettings)
#endif

	HANDLE_OPTION(bool, false, false, true, aimbot, "Aimbot", "Aim at enemy players", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, false, true, aimbotSetAngles, "Aim (silent)", "Set (silent) angles at the best aimbot target", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, false, true, aimbotSetViewAngles, "Aim (non-silent)", "Set camera angles when running aimbot", bindSettings, globalSettings)
	HANDLE_OPTION(int, 20, 0, 100, aimbotMinDamage, "Minimum Damage", "The minimum damage aimbot tries to achieve", bindSettings, globalSettings)
	HANDLE_OPTION(float, 360, 0, 360, aimbotFOV, "Aim FOV", "The effective range around the camera in which the aimbot will try to find targets.", bindSettings, globalSettings)
#ifdef TESTING_FEATURES
	HANDLE_OPTION(int, 0, 0, 2, aimbotLagCompensation, "Lag Compensation", "Predict enemy players into the future if necessary to hit (1 is low overhead, 2 is full)", bindSettings, globalSettings)
#endif
	HANDLE_OPTION(int, 0, 0, 100, aimbotHitChance, "Hit Chance", "Do not shoot if the chance to hit a player is below certain threshold", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, false, true, aimbotAutoShoot, "Auto Shoot", "Automatically shoot when the target was found", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, false, true, aimbotBacktrack, "Backtrack", "Try to move players back in time so they end up on the crosshair or somewhere aimable", bindSettings, globalSettings)
#ifdef TESTING_FEATURES
	HANDLE_OPTION(bool, false, false, true, aimbotSafeBacktrack, "Safe Backtrack", "Do not to set time lower than it was during previous tick (WIP, not functional)", bindSettings, globalSettings)
#endif
	HANDLE_OPTION(bool, false, false, true, aimbotNospread, "Nospread", "Compensate weapon inaccuracy (works only on servers supporting this feature)", bindSettings, globalSettings)
	HANDLE_OPTION(int, 400, 10, 1000, traceBudget, "Trace Budget", "The budget of game tracing (used in aimbot). Lower values yield better performance, yet higher values check more spots when running aimbot", bindSettings, globalSettings)
#ifdef TESTING_FEATURES
	HANDLE_OPTION(bool, false, false, true, resolver, "Resolver", "Try to resolve players desyncing animations", bindSettings, globalSettings)
	HANDLE_OPTION(int, 0, 0, 2, debugVisuals, "Debug Visuals", "Debug visuals (0 off, 1 low overhead, 2 all on)", bindSettings, globalSettings)
#ifdef MTR_ENABLED
	HANDLE_OPTION(bool, false, false, true, perfTrace, "PerfTrace", "Performance profile and save the result to a file (bind this to key)", bindSettings, globalSettings)
#endif
#endif
	HANDLE_OPTION(bool, false, false, true, rageMode, "Rage Mode", "Enable rage mode.", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, false, true, thirdPerson, "Third Person", "Enable third person", bindSettings, globalSettings)
#ifdef TESTING_FEATURES
	HANDLE_OPTION(bool, false, false, true, headCam, "Head Camera", "Draw camera in head position", bindSettings, globalSettings)
#endif
	HANDLE_OPTION(bool, false, false, true, thirdPersonShowReal, "Show Real", "Show real angle in third person instead of fake", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, false, true, disablePostProcessing, "NoPostFX", "Disable game's post processing effects", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, false, true, noFlash, "NoFlash", "Disable flashbang effect", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, false, true, noSmoke, "NoSmoke", "Disable smoke effect", bindSettings, globalSettings)
	HANDLE_OPTION(bool, false, false, true, noFog, "NoFog", "Disable in-game fog", bindSettings, globalSettings)
#undef HANDLE_OPTION
#else
static_assert(false, "HANDLE_OPTION not defined!");
#endif
