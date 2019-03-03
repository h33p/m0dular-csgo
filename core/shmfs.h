#ifndef SHMFS_H
#define SHMFS_H

#include "settings.h"

class SHMFSInstance
{
	struct SHMFSEntry
	{
		Settings::SHMemPtr<const char> buffer;
		uint32_t sz;
	};

	using FsPointer = Settings::SHMemPtr<SHMFSEntry>;
	template<typename T>
	using TAlloc = stateful_allocator<T, Settings::settingsAlloc>;

	boost::unordered_map<crcs_t, SHMFSEntry, boost::hash<crcs_t>, std::equal_to<crcs_t>, TAlloc<SHMFSEntry>> fsEntries;

	TAlloc<const char> alloc;

  public:
	SHMFSInstance();
	~SHMFSInstance();

	const SHMFSEntry& SetEntry(crcs_t crc, const char* buffer, size_t szIn);
    const SHMFSEntry* FindEntry(crcs_t crc);
	void RemoveEntry(crcs_t crc);
};

namespace SHMFS
{
	extern SHMFSInstance* sharedInstance;
}

#endif
