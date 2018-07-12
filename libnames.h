#ifndef LIBNAMES_H
#define LIBNAMES_H

#ifdef STACK_STRING
#define LIBNAME(var, value) const StackString var(value)
#else
#define LIBNAME(var, value) const char* var = value
#endif

#if defined(__linux__)
LIBNAME(clientLib, "client_panorama_client.so");
LIBNAME(engineLib, "engine_client.so");
LIBNAME(matSystemLib, "materialsystem_client.so");
LIBNAME(vguiLib, "vgui2_client.so");
LIBNAME(surfaceLib, "vguimatsurface_client.so");
LIBNAME(vstdLib, "libvstdlib_client.so");
#elif defined(__APPLE__)
LIBNAME(clientLib, "/client.dylib");
LIBNAME(engineLib, "/engine.dylib");
LIBNAME(matSystemLib, "/materialsystem.dylib");
LIBNAME(vguiLib, "/vgui2.dylib");
LIBNAME(surfaceLib, "/vguimatsurface.dylib");
LIBNAME(vstdLib, "/libvstdlib.dylib");
#elif defined(_WIN32) || defined(_WIN64)
LIBNAME(clientLib, "client_panorama.dll");
LIBNAME(engineLib, "engine.dll");
LIBNAME(matSystemLib, "materialsystem.dll");
LIBNAME(surfaceLib, "vguimatsurface.dll");
LIBNAME(vguiLib, "vgui2.dll");
LIBNAME(vstdLib, "vstdlib.dll");
#endif

#endif
