#ifndef S_SETTINGS_H
#define S_SETTINGS_H

#include "../sdk/framework/utils/settings.h"
#include "../sdk/framework/players.h"
#include "../sdk/framework/math/mmath.h"
#include "../sdk/framework/utils/shared_mutex.h"
#include "../sdk/source_csgo/studio.h"
#include "../sdk/framework/utils/freelistallocator.h"
#include "../sdk/framework/utils/allocwraps.h"

/*struct OptionBindData
{
	short key;
	char mode;
	bool active;

	OptionBindData()
		: key(0), mode(0), active(false)
	{

	}
};

//A packed allocator that injects additional data for bind management
class BindAllocator : public PackedAllocator
{
  public:

	idx_t Alloc(idx_t sz)
	{
		idx_t ret = PackedAllocator::Alloc(sz + sizeof(OptionBindData));
		*(OptionBindData*)(PackedAllocator::buf + ret + sz) = OptionBindData();
		return ret;
	}

	idx_t GetBindDataID(idx_t idx)
	{
		return idx + ((PackedAllocator::MetaData*)(PackedAllocator::buf + idx) - 1)->size - (idx_t)sizeof(OptionBindData);
	}

	OptionBindData& GetBindData(idx_t allocIdx)
	{
		return *(OptionBindData*)(PackedAllocator::buf + GetBindDataID(allocIdx));
	}
};

class BindSettingsGroup : public SettingsGroupBase<BindAllocator>
{
  public:

	//If the bind is not active, return null pointer so SettingsChain falls to the next option. If this object is the last in the chain, the program will crash - as intended
	template<typename T>
	constexpr T* RetreivePtrFast(idx_t idx)
	{
		if (!alloc.GetBindData(idx).active)
			return nullptr;

		return (T*)(alloc + idx);
	}

	template<typename T>
	constexpr T* RetreivePtrFastNoAuth(idx_t idx)
	{
		return (T*)(alloc + idx);
	}

	template<typename T>
	constexpr OptionBindData& GetBindData(idx_t idx)
	{
		return *(OptionBindData*)(alloc + idx + (idx_t)sizeof(T));
	}

	};*/

struct AimbotHitbox
{
	int hitbox = -1;
	unsigned char mask = 0;
	float pointScale = 0.5f;
};

namespace Settings
{
	extern uintptr_t allocBase;
	extern generic_free_list_allocator<allocBase>* settingsAlloc;

	extern SettingsGroupBase<stateful_allocator<unsigned char, settingsAlloc>>* globalSettingsPtr;
	extern pointer_proxy<globalSettingsPtr> globalSettings;
	extern SettingsGroupBase<stateful_allocator<unsigned char, settingsAlloc>>* bindSettingsPtr;
	extern pointer_proxy<globalSettingsPtr> bindSettings;
	extern SharedMutex* ipcLock;

	extern AimbotHitbox aimbotHitboxes[MAX_HITBOXES];

#define HANDLE_OPTION(type, defaultVal, name, ...) extern OPTION(type, name, __VA_ARGS__);
#include "option_list.h"
}

#endif
