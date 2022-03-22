#ifndef INTERFACES_H
#define INTERFACES_H

#include "../sdk/source_shared/interfaces.h"
#include "libnames.h"

#define INTERFACE(out, lib, name, exact) {(void*&)out, lib, name, exact}

const InterfaceDefinition interfaceList[] = {
	INTERFACE(cl, clientLib, "VClient", false),
	INTERFACE(entityList, clientLib, "VClientEntityList", false),
	INTERFACE(cvar, matSystemLib, "VEngineCvar", false),
	INTERFACE(engine, engineLib, "VEngineClient", false),
};

#endif
