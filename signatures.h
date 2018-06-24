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
	{(uintptr_t&)Weapon_ShootPosition, clientLib, "55 48 89 E5 53 48 89 FB 48 83 EC 38 E8 ? ? ? ? 80 BB"}
};
#elif defined(__APPLE__)
const Signature signatures[] = {
	{(uintptr_t&)clientMode, clientLib, "[48 8B 3D **? ? ? ?] 48 8B 07 5D FF A0 D8"},
	{(uintptr_t&)CL_RunPrediction, engineLib, "55 48 89 E5 53 50 E8 ? ? ? ? 48 89 C3 83 BB ? ? ? 00 06 75 1D"},
	{(uintptr_t&)RunSimulationFunc, clientLib, "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 18 49 89 CE 48 89 D3 F3 0F 11 45 D0"},
	{(uintptr_t&)Weapon_ShootPosition, clientLib, "55 48 89 E5 53 48 83 EC 18 48 89 FB E8 ? ? ? ? 0F 13 45 E8 F3 0F 11 4D F0 80 BB"}
};
#elif defined(_WIN32)
const Signature signatures[] = {
	{(uintptr_t&)clientMode, clientLib, "A1 *? ? ? ? 8B 80 ? ? ? ? 5D"},
	{(uintptr_t&)clientState, engineLib, "A1 *? ? ? ? 8B 80 ? ? ? ? C3"},
	{(uintptr_t&)CL_RunPrediction, engineLib, "57 8B 3D ? ? ? ? 83 BF 08 01 00 00 06 75 4A"},
	{(uintptr_t&)RunSimulationFunc, clientLib, "55 8B EC 83 EC 08 53 8B 5D 10 56"},
	{(uintptr_t&)Weapon_ShootPosition, clientLib, "55 8B EC 56 8B 75 08 57 8B F9 56 8B 07 FF ? ? ? ? ? 80"}
};
#endif

#endif
