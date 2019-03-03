#include "shmfs.h"

SHMFSInstance* SHMFS::sharedInstance = nullptr;

SHMFSInstance::SHMFSInstance()
	: fsEntries(), alloc() {}

SHMFSInstance::~SHMFSInstance()
{
	for (auto& i : fsEntries)
		alloc.deallocate(i.second.buffer, i.second.sz);

	fsEntries.clear();
}

const SHMFSInstance::SHMFSEntry& SHMFSInstance::SetEntry(crcs_t crc, const char* buffer, size_t szIn)
{
	uint32_t sz = (uint32_t)szIn;

	if (fsEntries.find(crc) != fsEntries.end())
		alloc.deallocate(fsEntries[crc].buffer, fsEntries[crc].sz);

	auto& ret = fsEntries[crc];

	ret = SHMFSEntry();
	ret.buffer = alloc.allocate(sz);
	ret.sz = sz;

	memcpy((char*)(const char*)ret.buffer, buffer, sz);

	return ret;
}

const SHMFSInstance::SHMFSEntry* SHMFSInstance::FindEntry(crcs_t crc)
{
	auto iter = fsEntries.find(crc);

	if (iter != fsEntries.end())
		return &(iter->second);

	return nullptr;
}

void SHMFSInstance::RemoveEntry(crcs_t crc)
{
	auto iter = fsEntries.find(crc);

	if (iter != fsEntries.end()) {
		alloc.deallocate(iter->second.buffer, iter->second.sz);
		fsEntries.erase(iter);
	}
}
