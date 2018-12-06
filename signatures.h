#ifndef SIGNATURES_H
#define SIGNATURES_H

#include "libnames.h"

struct Signature
{
	uintptr_t* result;
	const char* module;
	const char* pattern;

	template<typename T>
	constexpr Signature(T& ref, const char* lib, const char* sig) : result((uintptr_t*)(uintptr_t)&ref), module(lib), pattern(sig) {}
#ifdef STACK_STRING
	~Signature()
	{
		delete pattern;
	}
#endif
};

#ifdef STACK_STRING
#define SIGNATURE(out, lib, sig) Signature(out, lib, (new StackString(sig))->val())
#else
#define SIGNATURE(out, lib, sig) Signature(out, lib, sig)
#endif

const Signature signatures[] = {
#if defined(__linux__)
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
	SIGNATURE(Weapon_ShootPosition, clientLib, "55 48 89 E5 53 48 89 FB 48 83 EC 38 E8 ? ? ? ? 80 BB"),
	SIGNATURE(IntersectRayWithBox, engineLib, "[E8 *? ? ? ?] B8 01 00 00 00 E9 74 FB FF FF"),
	SIGNATURE(ClipRayToBSP, engineLib, "[E8 *? ? ? ?] 84 C0 0F 84 C8 FE"),
	SIGNATURE(ClipRayToOBB, engineLib, "E4 [E8 *? ? ? ?] E9 F4 FE"),
	SIGNATURE(ClipRayToVPhysics, engineLib, "[E8 *? ? ? ?] 3C 01 0F 84 48 01 00"),
	SIGNATURE(modelLoader, engineLib, "E9 B3 FE FF FF 66 90 [48 8B 05 ***? ? ? ?]"),
#elif defined(__APPLE__)
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
	SIGNATURE(Weapon_ShootPosition, clientLib, "55 48 89 E5 53 48 83 EC 18 48 89 FB E8 ? ? ? ? 0F 13 45 E8 F3 0F 11 4D F0 80 BB"),
	SIGNATURE(IntersectRayWithBox, engineLib, "B0 4D 89 F0 [E8 *? ? ? ?]"),
	SIGNATURE(IntersectRayWithOBB, engineLib, "4D 89 F1 [E8 *? ? ? ?] B0 01 EB 02"),
	SIGNATURE(CM_InlineModelNumber, engineLib, "[E8 *? ? ? ?] 48 85 C0 0F 84 1F 02"),
	SIGNATURE(TransformedBoxTrace, engineLib, "[E8 *? ? ? ?] EB 62 84"),
	SIGNATURE(ClipRayToVPhysics, engineLib, "[E8 *? ? ? ?] 4D 85 ED 74 56"),
	SIGNATURE(modelLoader, engineLib, "03 75 2D [48 8D 05 **? ? ? ?]"),
#elif defined(_WIN32)
	SIGNATURE(clientMode, clientLib, "A1 *? ? ? ? 8B 80 ? ? ? ? 5D"),
	SIGNATURE(clientState, engineLib, "A1 *? ? ? ? 8B 80 ? ? ? ? C3"),
	SIGNATURE(CL_RunPrediction, engineLib, "57 8B 3D ? ? ? ? 83 BF 08 01 00 00 06 75 4A"),
	SIGNATURE(RunSimulationFunc, clientLib, "55 8B EC 83 EC 08 53 8B 5D 10 56"),
	SIGNATURE(GetWeaponInfo, clientLib, "55 8B EC 83 EC 0C 53 8B 5D 08 89 4D F4 81 FB FF FF 00 00"),
	SIGNATURE(weaponDatabase, clientLib, "*? ? ? ? FF 10 0F B7 C0 B9 ? ? ? ? 50"),
	SIGNATURE(SetAbsOrigin, clientLib, "50 [E8 *? ? ? ?] EB 78 FF"),
	SIGNATURE(SetAbsAngles, clientLib, "8B CE [E8 *? ? ? ?] 8B 8E F4 09 00 00"),
	SIGNATURE(SetAbsVelocity, clientLib, "F3 0F 11 44 24 24 [E8 *? ? ? ?] 5F"),
	SIGNATURE(SetupBones, clientLib, "55 8B EC 83 E4 F0 B8 D8 1C 00 00 E8 ? ? ? ? 56"),
	SIGNATURE(effectsHead, clientLib, "8B 35 *? ? ? ? 85 F6 0F 84 ? ? ? ? BB FF FF 00 00 8B 0E"),
	SIGNATURE(Weapon_ShootPosition, clientLib, "55 8B EC 56 8B 75 08 57 8B F9 56 8B 07 FF ? ? ? ? ? 80"),
	SIGNATURE(IntersectRayWithBox, engineLib, "[E8 *? ? ? ?] 83 C4 0C 5F B0 01"),
	SIGNATURE(ClipRayToBSP, engineLib, "53 [E8 *? ? ? ?] 84 C0 75 09"),
	SIGNATURE(ClipRayToOBB, engineLib, "53 [E8 *? ? ? ?] 8A 54"),
	SIGNATURE(ClipRayToVPhysics, engineLib, "53 [E8 *? ? ? ?] 84 C0 75 29"),
	SIGNATURE(modelLoader, engineLib, "75 2F 50 A1 *"),
#endif
};

#endif
