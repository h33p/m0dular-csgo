#include "binds.h"

static constexpr int MAX_BIND_ELEMENTS = 100;
static constexpr uintptr_t constAllocBase = 0;
generic_free_list_allocator<constAllocBase> bindAlloc(MAX_BIND_ELEMENTS * (sizeof(*BindManager::bindList[0]) + sizeof(uint64_t) + 4), PlacementPolicy::FIND_FIRST);

BindHandlerIFace* BindManager::bindList[] =
{
#define HANDLE_OPTION(type, defaultValue, name, ...) new (bindAlloc.allocate<BindImpl<type>>(1)) BindImpl<type>((Settings::bindSettings.operator->()), CCRC32(#name)),
#include "option_list.h"
};

std::vector<unsigned char> BindManager::SerializeBinds()
{
	std::vector<unsigned char> ret;

	return ret;
}

void BindManager::LoadBinds(const std::vector<unsigned char>& vec)
{

}
