#ifndef LIBNAMES_H
#define LIBNAMES_H

#if defined(__linux__)
#define STRTOLIB(LIB) LIB "_client.so"
#elif defined(__APPLE__)
#define STRTOLIB(LIB) "/" LIB ".dylib"
#elif defined(_WIN32) || defined (_WIN64)
#define STRTOLIB(LIB) LIB ".dll"
#endif

#ifdef STACK_STRING
#define LIBNAME(var, value) const StackString var(STRTOLIB(value))
#else
#define LIBNAME(var, value) const char* var = STRTOLIB(value)
#endif

LIBNAME(clientLib, "client_panorama");
LIBNAME(engineLib, "engine");
LIBNAME(matSystemLib, "materialsystem");
LIBNAME(surfaceLib, "vguimatsurface");
LIBNAME(vguiLib, "vgui2");
LIBNAME(vstdLib, OPosix("lib") "vstdlib");
LIBNAME(datacacheLib, "datacache");
LIBNAME(physicsLib, "vphysics");
LIBNAME(tierLib, OPosix("lib")"tier0");
LIBNAME(shaderapiLib, "shaderapidx9");
LIBNAME(serverLib, "server");
LIBNAME(studioLib, "studiorender");

#endif
