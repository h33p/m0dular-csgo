#ifndef S_SETTINGS_H
#define S_SETTINGS_H

#include "../sdk/framework/utils/settings.h"
#include "../sdk/framework/players.h"
#include "../sdk/framework/math/mmath.h"
#include "../sdk/source_csgo/studio.h"

struct OptionBindData
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
		*(OptionBindData*)(PackedAllocator::buf + sz) = OptionBindData();
		return ret;
	}

	idx_t GetBindDataID(idx_t idx)
	{
		return idx + ((PackedAllocator::MetaData*)(PackedAllocator::buf + idx) - 1)->size - sizeof(OptionBindData);
	}

	OptionBindData GetBindData(idx_t allocIdx)
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

};

struct AimbotHitbox
{
	int hitbox = -1;
	unsigned char mask = 0;
	float pointScale = 0.5f;
};

namespace Settings
{
	extern SettingsGroup globalSettings;
	extern SettingsGroup bindSettings;

	extern OPTION(bool, bunnyhopping, bindSettings, globalSettings);
	extern OPTION(bool, autostrafer, bindSettings, globalSettings);
	extern OPTION(bool, antiaim, bindSettings, globalSettings);

	extern OPTION(bool, fakelag, bindSettings, globalSettings);

	extern OPTION(bool, aimbot, bindSettings, globalSettings);
	extern OPTION(bool, aimbotSetAngles, bindSettings, globalSettings);
	extern OPTION(int, aimbotLagCompensation, bindSettings, globalSettings);
	extern OPTION(int, aimbotHitChance, bindSettings, globalSettings);
	extern OPTION(bool, aimbotAutoShoot, bindSettings, globalSettings);
	extern OPTION(bool, aimbotBacktrack, bindSettings, globalSettings);
	extern OPTION(bool, aimbotSafeBacktrack, bindSettings, globalSettings);
	extern OPTION(bool, aimbotNospread, bindSettings, globalSettings);
	extern AimbotHitbox aimbotHitboxes[MAX_HITBOXES];
}

#endif
