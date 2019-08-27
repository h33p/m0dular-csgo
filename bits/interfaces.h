#ifndef INTERFACES_H
#define INTERFACES_H

#include "../sdk/source_shared/interfaces.h"
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
	INTERFACE(gameEvents, engineLib, "GAMEEVENTSMANAGER002", true),
	INTERFACE(debugOverlay, engineLib, "VDebugOverlay", false),
	INTERFACE(mdlCache, datacacheLib, "MDLCache", false),
	INTERFACE(spatialPartition, engineLib, "SpatialPartition", false),
	INTERFACE(staticPropMgr, engineLib, "StaticPropMgrClient", false),
	INTERFACE(physProp, physicsLib, "VPhysicsSurfaceProps", false),
	INTERFACE(gameMovement, clientLib, "GameMovement", false),

	INTERFACE(server, serverLib, "ServerGameDLL", false),
};

#endif
