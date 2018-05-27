#ifndef SIGNATURES_H
#define SIGNATURES_H

typedef struct
{
	uintptr_t& result;
	const char* module;
	const char* pattern;
} Signature;

#if defined(__linux__)
const Signature signatures[] = {
	{(uintptr_t&)clientMode, "client_client.so", "[48 8D 05 *? ? ? ?] 48 89 E5 48 89 05 ? ? ? ? E8 ? ? ? ? 5D 48"},
	{(uintptr_t&)Weapon_ShootPosition, "client_client.so", "55 48 89 E5 53 48 89 FB 48 83 EC 38 E8 ? ? ? ? 80 BB"}
};
#elif defined(__APPLE__)
const Signature signatures[] = {
	{(uintptr_t&)clientMode, "/client.dylib", "[48 8B 3D **? ? ? ?] 48 8B 07 5D FF A0 D8"},
	{(uintptr_t&)Weapon_ShootPosition, "/client.dylib", "55 48 89 E5 53 48 83 EC 18 48 89 FB E8 ? ? ? ? 0F 13 45 E8 F3 0F 11 4D F0 80 BB"}
};
#elif defined(_WIN32)
const Signature signatures[] = {
	{(uintptr_t&)clientMode, "client.dll", "A1 *? ? ? ? 8B 80 ? ? ? ? 5D"},
	{(uintptr_t&)Weapon_ShootPosition, "client.dll", "55 8B EC 56 8B 75 08 57 8B F9 56 8B 07 FF ? ? ? ? ? 80"}
};
#endif

#endif
