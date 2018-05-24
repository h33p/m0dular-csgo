#ifndef SIGNATURES_H
#define SIGNATURES_H

typedef struct
{
	const char* module;
	const char* pattern;
} signature_t;

#if defined(__linux__)
signature_t clientModeSig = {"client_client.so", "[48 8D 05 *? ? ? ?] 48 89 E5 48 89 05 ? ? ? ? E8 ? ? ? ? 5D 48"};
#elif defined(__APPLE__)
signature_t clientModeSig = {"client.dylib", "[48 8B 3D **? ? ? ?] 48 8B 07 5D FF A0 D8"};
#elif defined(_WIN32)
signature_t clientModeSig = {"client.dll", "A1 *? ? ? ? 8B 80 ? ? ? ? 5D"};
#endif

#endif
