#ifndef SIGNATURES_H
#define SIGNATURES_H

#include "libnames.h"

typedef struct
{
	uintptr_t& result;
	const char* module;
	const char* pattern;
} Signature;

#if defined(__linux__)
const Signature signatures[] = {
	{(uintptr_t&)clientMode, clientLib, "[48 8D 05 *? ? ? ?] 48 89 E5 48 89 05 ? ? ? ? E8 ? ? ? ? 5D 48"},
	{(uintptr_t&)CL_RunPrediction, engineLib, "55 48 89 E5 53 48 83 EC 08 E8 ? ? ? ? 83 B8 A0 01 00 00 06 48 89 C3"},
	{(uintptr_t&)RunSimulationFunc, clientLib, "55 48 89 E5 41 57 49 89 CF 41 56 49 89 FE 41 55 41 54 41 89 F4 53"},
	{(uintptr_t&)GetWeaponInfo, clientLib, "55 48 89 E5 41 57 41 56 41 55 41 89 F5 41 54 45 31 E4 53"},
	{(uintptr_t&)weaponDatabase, clientLib, "74 32 [48 8B 05 **-24? ? ? ?] 48 8B 00"},
	{(uintptr_t&)SetAbsOrigin, clientLib, "55 48 89 E5 48 89 5D E8 48 89 FB 4C 89 65 F0 49 89 F4 4C 89 6D F8 48 83 EC 20 E8 B1 ? ? ? F3 41 0F"},
	{(uintptr_t&)SetAbsAngles, clientLib, "55 48 89 E5 48 89 5D E0 48 89 FB 4C 89 6D F0 49 89 F5 4C 89 65 E8 4C 89 75 F8 48 ? ? ? E8 ? ? ? ? F3"},
	{(uintptr_t&)SetAbsVelocity, clientLib, "55 48 89 E5 48 89 5D E8 48 89 FB 4C 89 65 F0 49 89 F4 4C 89 6D F8 48 83 EC 30 F3 0F 10 06 0F 2F 87 CC 00 00 00 75 12"},
	{(uintptr_t&)SetupBones, clientLib, "55 48 8D 05 ? ? ? ? 48 89 E5 41 57 41 56 41 55 4C 8D 2D"},
	{(uintptr_t&)Weapon_ShootPosition, clientLib, "55 48 89 E5 53 48 89 FB 48 83 EC 38 E8 ? ? ? ? 80 BB"}
};
#elif defined(__APPLE__)
const Signature signatures[] = {
	{(uintptr_t&)clientMode, clientLib, "[48 8B 3D **? ? ? ?] 48 8B 07 5D FF A0 D8"},
	{(uintptr_t&)CL_RunPrediction, engineLib, "55 48 89 E5 53 50 E8 ? ? ? ? 48 89 C3 83 BB ? ? ? 00 06 75 1D"},
	{(uintptr_t&)RunSimulationFunc, clientLib, "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 18 49 89 CE 48 89 D3 F3 0F 11 45 D0"},
	{(uintptr_t&)GetWeaponInfo, clientLib, "55 48 89 E5 53 50 31 C0 85 FF 74 74"},
	{(uintptr_t&)SetAbsOrigin, clientLib, "55 48 89 E5 41 57 41 56 53 50 49 89 F6 48 89 FB E8 ? ? ? ? F3 41 0F 10 06"},
	{(uintptr_t&)SetAbsAngles, clientLib, "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 68 49 89 F6 48 89 FB E8 ? ? ? ? F3 41 0F 10 06"},
	{(uintptr_t&)SetAbsVelocity, clientLib, "55 48 89 E5 41 57 41 56 53 48 83 EC 18 49 89 F6 48 89 FB F3 41 0F 10 06"},
	{(uintptr_t&)Weapon_ShootPosition, clientLib, "55 48 89 E5 53 48 83 EC 18 48 89 FB E8 ? ? ? ? 0F 13 45 E8 F3 0F 11 4D F0 80 BB"}
};
#elif defined(_WIN32)
const Signature signatures[] = {
	{(uintptr_t&)clientMode, clientLib, "A1 *? ? ? ? 8B 80 ? ? ? ? 5D"},
	{(uintptr_t&)clientState, engineLib, "A1 *? ? ? ? 8B 80 ? ? ? ? C3"},
	{(uintptr_t&)CL_RunPrediction, engineLib, "57 8B 3D ? ? ? ? 83 BF 08 01 00 00 06 75 4A"},
	{(uintptr_t&)RunSimulationFunc, clientLib, "55 8B EC 83 EC 08 53 8B 5D 10 56"},
	{(uintptr_t&)GetWeaponInfo, clientLib, "55 8B EC 83 EC 0C 53 8B 5D 08 89 4D F4 81 FB FF FF 00 00"},
	{(uintptr_t&)weaponDatabase, clientLib, "*? ? ? ? FF 10 0F B7 C0 B9 ? ? ? ? 50"},
	{(uintptr_t&)SetAbsOrigin, clientLib, "55 8B EC 83 E4 F8 51 53 56 57 8B F1"},
	{(uintptr_t&)SetAbsAngles, clientLib, "55 8B EC 83 E4 F8 83 EC 64 53 56 57 8B F1 E8"},
	{(uintptr_t&)SetAbsVelocity, clientLib, "55 8B EC 83 E4 F8 83 EC 0C 53 56 57 8B 7D 08 8B F1"},
	{(uintptr_t&)Weapon_ShootPosition, clientLib, "55 8B EC 56 8B 75 08 57 8B F9 56 8B 07 FF ? ? ? ? ? 80"}
};
#endif

#endif
