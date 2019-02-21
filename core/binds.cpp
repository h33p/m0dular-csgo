#include "binds.h"

BindManagerInstance* BindManager::sharedInstance = nullptr;

BindManagerInstance::BindManagerInstance()
	: cstart(__COUNTER__ + 1), bindList {
#define HANDLE_OPTION(type, defaultValue, name, ...) new (Settings::settingsAlloc->allocate<BindImpl<type>>(1)) BindImpl<type>(__COUNTER__ - cstart, (Settings::bindSettings.operator->()), CCRC32(#name), type()),
#include "../bits/option_list.h"
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
	BindHandlerIFaceVtable* vtbl = Settings::settingsLocalAlloc.allocate<BindHandlerIFaceVtable>(Settings::optionCount);
#define HANDLE_OPTION(type, defaultValue, name, ...) ((BindImpl<type>*)&*bindList[cnt])->InitializeVTable(vtbl + cnt); cnt++;
#include "../bits/option_list.h"
}



void BindKey::Serialize(std::vector<unsigned char>& vec)
{
	vec.push_back((unsigned char)mode);

	if (pointer) {
		for (size_t i = 0; i < sizeof(int); i++)
			vec.push_back(((unsigned char*)&pointer->handler->id)[i]);
		pointer->Serialize(vec);
	} else
		for (size_t i = 0; i < sizeof(int); i++)
			vec.push_back((unsigned char)~0u);
}

size_t BindKey::Unserialize(const std::vector<unsigned char>& vec, size_t idx)
{
	mode = (BindMode)vec[idx++];

	if (pointer)
		pointer->~BindDataIFace();

	pointer = nullptr;

	int id = -1;

	for (size_t i = 0; i < sizeof(int); i++)
		((unsigned char*)&id)[i] = vec[idx++];

	if (id >= 0) {
		pointer = BindManager::sharedInstance->bindList[id]->AllocKeyBind();
		idx = pointer->Unserialize(vec, idx);
	}

	return idx;
}

void BindKey::HandleKeyPress(bool isDown)
{
	if (!pointer) {
		down = false;
		state = 0;
		return;
	}

	if (down == isDown)
		return;

	down = isDown;

	bool enable = false;

	switch(mode) {
	  case BindMode::HOLD:
		  enable = down;
		  break;
	  case BindMode::TOGGLE:
		  if (!down) {
			  state = (char)!state;
			  enable = state;
		  } else
			  return;
		  break;
	}

	if (enable)
		pointer->HandleEnable();
	else
		pointer->HandleDisable();
}

void BindKey::Unbind()
{
	if (pointer) {
		pointer->~BindDataIFace();
		pointer = nullptr;
	}
}


static constexpr unsigned char BINDS_MAGIC = 0xfe;

void BindManager::SerializeBinds(std::vector<unsigned char>& vec)
{
	vec.push_back(BINDS_MAGIC);

	for (int i = 0; i < 255; i++) {
		if (sharedInstance->binds[i].pointer) {
			vec.push_back((unsigned char)i);
			sharedInstance->binds[i].Serialize(vec);
		}
	}

	vec.push_back((unsigned char)255);
}

size_t BindManager::LoadBinds(const std::vector<unsigned char>& vec, size_t idx)
{
    if (vec[idx++] != BINDS_MAGIC)
		return --idx;

	for (int i = 0; i < 255; i++)
		sharedInstance->binds[i].Unbind();

	while (1) {
		unsigned char id = vec[idx++];

		if (id == 255)
			break;

		idx = sharedInstance->binds[id].Unserialize(vec, idx);
	}

	return idx;
}
