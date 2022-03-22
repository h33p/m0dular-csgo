#ifndef LIBNAMES_H
#define LIBNAMES_H

#if defined(__linux__)
#define STRTOLIB(LIB) LIB "_client.so"
#elif defined(__APPLE__)
#define STRTOLIB(LIB) "/" LIB ".dylib"
#elif defined(_WIN32) || defined (_WIN64)
#define STRTOLIB(LIB) LIB ".dll"
#endif

#define LIBNAME(var, value) const char* var = STRTOLIB(value)

LIBNAME(clientLib, "client");
LIBNAME(engineLib, "engine");
LIBNAME(matSystemLib, "materialsystem");

#endif
