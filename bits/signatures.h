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
};

#define SIGNATURE(out, lib, sig) Signature(out, lib, sig)

const Signature signatures[] = {
#if defined(__linux__)
	SIGNATURE(clientMode, clientLib, "[48 8D 05 *? ? ? ?] 48 89 E5 48 89 05 ? ? ? ? E8 ? ? ? ? 5D 48"),
	SIGNATURE(postProcessDisable, clientLib, "[80 3D *? ? ? ? 00] 89 B5 14 FF"),
	SIGNATURE(smokeCount, clientLib, "FF E9 54 FD FF FF [E8 *+1:2,10 ? ? ? ?]"),
	SIGNATURE(glowObjectManager, clientLib, "85 C0 [0F 84 *:3,7 98 00 00 00] 48 8D 3D"),
#elif defined(__APPLE__)
	SIGNATURE(clientMode, clientLib, "[48 8B 3D **? ? ? ?] 48 8B 07 5D FF A0 D8"),
	SIGNATURE(postProcessDisable, clientLib, ""),
	SIGNATURE(smokeCount, clientLib, ""),
	SIGNATURE(glowObjectManager, clientLib, ""),
#elif defined(_WIN32)
	SIGNATURE(clientMode, clientLib, "B9 *? ? ? ? E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? 53"),
	SIGNATURE(postProcessDisable, clientLib, "80 3D *? ? ? ? ? 53 56 57 0F 85"),
	SIGNATURE(smokeCount, clientLib, "A3 *? ? ? ? 57 8B CB"),
	SIGNATURE(glowObjectManager, clientLib, "0F 11 05 *? ? ? ? 83 C8 01"),
#endif
};

#endif
