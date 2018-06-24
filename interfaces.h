#ifndef INTERFACES_H
#define INTERFACES_H

#include "sdk/source_shared/interfaces.h"
#include "libnames.h"

const InterfaceDefinition interfaceList[] = {
	{(void*&)cl, clientLib, "VClient", false},
	{(void*&)engine, engineLib, "VEngineClient", false},
	{(void*&)mdlInfo, engineLib, "VModelInfoClient", false},
	{(void*&)entityList, clientLib, "VClientEntityList", false},
	{(void*&)engineTrace, engineLib, "EngineTraceClient", false},
	{(void*&)cvar, matSystemLib, "VEngineCvar", false},
	{(void*&)prediction, clientLib, "VClientPrediction", false},
};

#endif
