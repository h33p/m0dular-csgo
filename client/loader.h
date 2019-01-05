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
	char game_name[32];
	char int_name[32];

	SubscriptionEntry(const char* in_name, const char* in_sub_date, const char* in_game_name, const char* in_int_name)
	{
		strncpy(name, in_name, 127);
		strncpy(subscription_date, in_sub_date, 63);
		strncpy(game_name, in_game_name, 31);
		strncpy(int_name, in_int_name, 31);
	}
};

extern std::vector<SubscriptionEntry> subscriptionList;

void Load();
void ServerReceiveModule(const char* dataIn, uint32_t dataSize);
uint64_t ServerAllocateModule(uint32_t allocSize);

#endif
