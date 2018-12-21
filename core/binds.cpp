#include "binds.h"

BindManagerInstance* BindManager::sharedInstance = nullptr;

BindManagerInstance::BindManagerInstance()
	: bindList {
#define HANDLE_OPTION(type, defaultValue, name, ...) new (Settings::settingsAlloc->allocate<BindImpl<type>>(1)) BindImpl<type>((Settings::bindSettings.operator->()), CCRC32(#name), type()),
#include "option_list.h"
}, binds {}
{
}

BindManagerInstance::~BindManagerInstance()
{
	for (auto& i : binds)
		if (i.pointer)
			i.pointer->~BindDataIFace();

	for (auto& i : bindList) {
		//Technically UB, but settingsAlloc does know how to handle any type
		if (i)
			Settings::settingsAlloc->deallocate(i, 1);
	}
}

void BindManagerInstance::InitializeLocalData()
{
	int cnt = 0;
#define HANDLE_OPTION(type, defaultValue, name, ...) ((BindImpl<type>*)&*bindList[cnt++])->InitializeVTable();
#include "option_list.h"
}

std::vector<unsigned char> BindManager::SerializeBinds()
{
	std::vector<unsigned char> ret;

	return ret;
}

void BindManager::LoadBinds(const std::vector<unsigned char>& vec)
{

}
