#ifndef INTERFACES_H
#define INTERFACES_H

#include "sdk/source_shared/interfaces.h"
#include "libnames.h"

#ifdef STACK_STRING
#define INTERFACE(out, lib, name, exact) {(void*&)out, lib, (new StackString(name))->val(), exact}
#else
#define INTERFACE(out, lib, name, exact) {(void*&)out, lib, name, exact}
#endif

const InterfaceDefinition interfaceList[] = {
	INTERFACE(cl, clientLib, "VClient", false),
	INTERFACE(engine, engineLib, "VEngineClient", false),
	INTERFACE(mdlInfo, engineLib, "VModelInfoClient", false),
	INTERFACE(entityList, clientLib, "VClientEntityList", false),
	INTERFACE(engineTrace, engineLib, "EngineTraceClient", false),
	INTERFACE(cvar, matSystemLib, "VEngineCvar", false),
	INTERFACE(prediction, clientLib, "VClientPrediction", false),
	INTERFACE(panel, vguiLib, "VGUI_Panel", false),
	INTERFACE(surface, surfaceLib, "VGUI_Surface", false),
};

#endif
