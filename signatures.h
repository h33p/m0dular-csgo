#ifndef SIGNATURES_H
#define SIGNATURES_H

#include "libnames.h"

struct Signature
{
	uintptr_t& result;
	const char* module;
	const char* pattern;

#ifdef STACK_STRING
	~Signature()
	{
		delete pattern;
	}
#endif
};

#ifdef STACK_STRING
#define SIGNATURE(out, lib, sig) {(uintptr_t&)out, lib, (new StackString(sig))->val()}
#else
#define SIGNATURE(out, lib, sig) {(uintptr_t&)out, lib, sig}
#endif

#if defined(__linux__)
const Signature signatures[] = {
	SIGNATURE(clientMode, clientLib, "[48 8D 05 *? ? ? ?] 48 89 E5 48 89 05 ? ? ? ? E8 ? ? ? ? 5D 48"),
	SIGNATURE(CL_RunPrediction, engineLib, "F3 0F 11 40 24 [E8 *? ? ? ?] E8 ? ? ? ? BF 01"),
	SIGNATURE(RunSimulationFunc, clientLib, "[E8 *? ? ? ?] 48 8B 05 ? ? ? ? 0F 57 C0"),
	SIGNATURE(GetWeaponInfo, clientLib, "55 48 89 E5 41 57 41 56 41 55 41 54 53 89 F3 48 83 EC 28"),
	SIGNATURE(weaponDatabase, clientLib, "FF 10 [48 8B 3D **-24? ? ? ?] 48 83 C4 08"),
	SIGNATURE(SetAbsOrigin, clientLib, "48 89 DF [E8 *? ? ? ?] B8 01 00 00 00 E9 06"),
	SIGNATURE(SetAbsAngles, clientLib, "48 89 DF [E8 *? ? ? ?] B8 01 00 00 00 E9 49"),
	SIGNATURE(SetAbsVelocity, clientLib, "[E8 *? ? ? ?] EB 22 0F 1F 40 00"),
	SIGNATURE(SetupBones, clientLib, "5B 44 89 E1 4C 89 EE 44 89 F2 41 5C 41 5D 41 5E 5D [E9 *? ? ? ?]"),
	SIGNATURE(effectsHead, clientLib, "75 1E [48 8B 1D *? ? ? ?] 48 85 DB 0F"),
	SIGNATURE(Weapon_ShootPosition, clientLib, "55 48 89 E5 53 48 89 FB 48 83 EC 38 E8 ? ? ? ? 80 BB")
};
#elif defined(__APPLE__)
const Signature signatures[] = {
	SIGNATURE(clientMode, clientLib, "[48 8B 3D **? ? ? ?] 48 8B 07 5D FF A0 D8"),
	SIGNATURE(CL_RunPrediction, engineLib, "55 48 89 E5 53 50 E8 ? ? ? ? 48 89 C3 83 BB ? ? ? 00 06 75 1D"),
	SIGNATURE(RunSimulationFunc, clientLib, "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 18 49 89 CE 48 89 D3 F3 0F 11 45 D0"),
	SIGNATURE(GetWeaponInfo, clientLib, "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 18 41 89 F6 49 89 FC 41 81 FE FE FF 00 00"),
	SIGNATURE(weaponDatabase, clientLib, "4C 89 F7 E8 ? ? ? ? 48 85 C0 75 12 [48 8D 05 **-24? ? ? ?] 48 8B 38"),
	SIGNATURE(SetAbsOrigin, clientLib, "55 48 89 E5 41 57 41 56 53 50 49 89 F6 48 89 FB E8 ? ? ? ? F3 41 0F 10 06"),
	SIGNATURE(SetAbsAngles, clientLib, "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 68 49 89 F6 48 89 FB E8 ? ? ? ? F3 41 0F 10 06"),
	SIGNATURE(SetAbsVelocity, clientLib, "55 48 89 E5 41 57 41 56 53 48 83 EC 18 49 89 F6 48 89 FB F3 41 0F 10 06"),
	SIGNATURE(SetupBones, clientLib, "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 81 EC ? ? ? ? F3 ? ? ? ? ? ? ? 89 CB 89 95"),
	SIGNATURE(effectsHead, clientLib, "[4C 8B 3D *? ? ? ?] 4D 85 FF 0F 84 ? ? ? ? 48 8D 1D"),
	SIGNATURE(viewRender, clientLib, "[48 89 05 *? ? ? ?] 48 C7 05 ? ? ? ? 00 00 00 00 48 8D ? ? ? ? ? 48 8D ? ? ? ? ? 48 89 DE 48 83 C4 08"),
	SIGNATURE(Weapon_ShootPosition, clientLib, "55 48 89 E5 53 48 83 EC 18 48 89 FB E8 ? ? ? ? 0F 13 45 E8 F3 0F 11 4D F0 80 BB")
};
#elif defined(_WIN32)
const Signature signatures[] = {
	SIGNATURE(clientMode, clientLib, "A1 *? ? ? ? 8B 80 ? ? ? ? 5D"),
	SIGNATURE(clientState, engineLib, "A1 *? ? ? ? 8B 80 ? ? ? ? C3"),
	SIGNATURE(CL_RunPrediction, engineLib, "57 8B 3D ? ? ? ? 83 BF 08 01 00 00 06 75 4A"),
	SIGNATURE(RunSimulationFunc, clientLib, "55 8B EC 83 EC 08 53 8B 5D 10 56"),
	SIGNATURE(GetWeaponInfo, clientLib, "55 8B EC 83 EC 0C 53 8B 5D 08 89 4D F4 81 FB FF FF 00 00"),
	SIGNATURE(weaponDatabase, clientLib, "*? ? ? ? FF 10 0F B7 C0 B9 ? ? ? ? 50"),
	SIGNATURE(SetAbsOrigin, clientLib, "55 8B EC 83 E4 F8 51 53 56 57 8B F1"),
	SIGNATURE(SetAbsAngles, clientLib, "55 8B EC 83 E4 F8 83 EC 64 53 56 57 8B F1 E8"),
	SIGNATURE(SetAbsVelocity, clientLib, "55 8B EC 83 E4 F8 83 EC 0C 53 56 57 8B 7D 08 8B F1"),
	SIGNATURE(SetupBones, clientLib, "55 8B EC 83 E4 F0 B8 D8 1C 00 00 E8 ? ? ? ? 56"),
	SIGNATURE(effectsHead, clientLib, "8B 35 *? ? ? ? 85 F6 0F 84 ? ? ? ? BB FF FF 00 00 8B 0E"),
	SIGNATURE(Weapon_ShootPosition, clientLib, "55 8B EC 56 8B 75 08 57 8B F9 56 8B 07 FF ? ? ? ? ? 80")
};
#endif

#endif
