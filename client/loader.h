#ifndef LOADER_H
#define LOADER_H

#include <stdint.h>
#include <vector>
#include <string.h>

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
	ModuleList() {}
};

struct SubscriptionEntry
{
	char name[128];
	char subscription_date[64];
	char int_name[32];

	SubscriptionEntry(const char* in_name, const char* in_sub_date, const char* in_int_name)
	{
		strncpy(name, in_name, 127);
		strncpy(subscription_date, in_sub_date, 63);
		strncpy(int_name, in_int_name, 31);
	}
};

struct ProcessEntry
{
	long long pid;
	char name[100];
	//TODO: Add process path

	ProcessEntry(long long p, const char* n)
	{
		pid = p;
		strncpy(name, n, 99);
	}
};

struct ModuleExport
{
	uint32_t crc;
	uint32_t baseOffset;

	ModuleExport()
		: crc(0), baseOffset(0) {}

	ModuleExport(uint32_t c, uint32_t off)
		: crc(c), baseOffset(off) {}
};

struct ModuleEntry
{
	uint64_t handle;
	uint64_t baseAddress;
	uint32_t size;
	int moduleID;
	std::vector<ModuleExport> exports;

	ModuleEntry(uint64_t h, uint64_t baddr, uint32_t sz, int mid, ModuleExport* exp, size_t expCount)
	{
		handle = h;
		baseAddress = baddr;
		size = sz;
		moduleID = mid;

		exports.reserve(expCount);

		for (size_t i = 0; i < expCount; i++)
			exports.push_back(exp[i]);
	}

	const ModuleExport* FindExport(uint32_t crc)
	{
		for (const ModuleExport& i : exports)
			if (i.crc == crc)
				return &i;
		return nullptr;
	}
};

extern std::vector<SubscriptionEntry> subscriptionList;
extern std::vector<ModuleEntry> loadedModules;

int Load(int loadID);
int LoadCheatMenu(int loadID);
void UnloadModule(long libID);
void ServerStartLoad(long pid);
void ServerReceiveModule(const char* dataIn, uint32_t dataSize);
uint64_t ServerAllocateModule(uint32_t allocSize);
void ServerUnloadModule(int libID);

#endif
