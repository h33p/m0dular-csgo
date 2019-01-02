#ifndef LOADER_H
#define LOADER_H

#include <stdint.h>
#include <vector>

struct RemoteModuleInfo
{
	uint64_t handle;
	uint64_t startAddress;
	uint64_t endAddress;
	uint32_t nameOffset;
};

struct ModuleList
{
	std::vector<char> names;
	std::vector<RemoteModuleInfo> modules;

	ModuleList(int64_t pid);
};

void Load();

#endif
