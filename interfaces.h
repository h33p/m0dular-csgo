#ifndef INTERFACES_H
#define INTERFACES_H

#include "sdk/source_shared/interfaces.h"

#if defined(__linux__)
const char* clientLib = "client_client.so";
const char* engineLib = "engine_client.so";
const char* matSystemLib = "materialsystem_client.so";
#elif defined(__APPLE__)
const char* clientLib = "/client.dylib";
const char* engineLib = "/engine.dylib";
const char* matSystemLib = "/materialsystem.dylib";
#elif defined(_WIN32) || defined(_WIN64)
const char* clientLib = "client.dll";
const char* engineLib = "engine.dll";
const char* matSystemLib = "materialsystem.dll";
#endif

const InterfaceDefinition interfaceList[] = {
	{(void*&)cl, clientLib, "VClient", false},
	{(void*&)engine, engineLib, "VEngineClient", false},
	{(void*&)mdlInfo, engineLib, "VModelInfoClient", false},
	{(void*&)entityList, clientLib, "VClientEntityList", false},
	{(void*&)engineTrace, engineLib, "EngineTraceClient", false},
	{(void*&)cvar, matSystemLib, "VEngineCvar", false},
};

#endif
