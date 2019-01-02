#ifndef S_SETTINGS_H
#define S_SETTINGS_H

#include "../sdk/framework/utils/settings.h"
#include "../sdk/framework/players.h"
#include "../sdk/framework/math/mmath.h"
#include "../sdk/framework/utils/shared_mutex.h"
#include "../sdk/source_csgo/studio.h"
#include "../sdk/framework/utils/freelistallocator.h"
#include "../sdk/framework/utils/allocwraps.h"

#ifdef _WIN32
#define offset_of(x, t) offsetof(x, t)
using fileHandle = HANDLE;
#else
#define offset_of(x, t) (__builtin_constant_p(offsetof(x, t) ? offsetof(x, t) : offsetof(x, t)))
using fileHandle = int;
#endif

//A packed allocator that injects additional data for bind management
template<typename Alloc, bool recursRebind>
class BindAllocator : public Alloc
{
  public:
	using value_type = typename Alloc::value_type;
	using pointer = typename std::allocator_traits<Alloc>::pointer;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using is_always_equal = typename std::allocator_traits<Alloc>::is_always_equal;
	using propagate_on_container_move_assignment = typename std::allocator_traits<Alloc>::propagate_on_container_move_assignment;

	struct real_value_type
	{
		bool bindActive;
	    value_type value;
	};

	static constexpr size_t bindOffset = offset_of(real_value_type, value);
	using RealAlloc = typename Alloc::template rebind<real_value_type>::other;
	using real_pointer = typename RealAlloc::pointer;
	using ByteAlloc = typename Alloc::template rebind<unsigned char>::other;
	using byte_pointer = typename ByteAlloc::pointer;

  public:

    pointer allocate(size_type sz)
	{
	    byte_pointer ret = ByteAlloc::allocate(sizeof(value_type) * sz + bindOffset);
		((real_pointer)ret)->bindActive = false;
		return (pointer)(ret + bindOffset);
	}

	void deallocate(pointer ptr, size_type sz)
	{
		byte_pointer bptr = (byte_pointer)ptr;
		bptr -= bindOffset;
		deallocate(bptr, sizeof(value_type) * sz + bindOffset);
	}

	inline bool operator==(const BindAllocator& o)
	{
		return Alloc::operator==(o);
	}

	template<typename Other>
	struct rebind
	{
		using other = typename std::conditional<recursRebind, BindAllocator<typename Alloc::template rebind<Other>::other, recursRebind>, typename Alloc::template rebind<Other>::other>::type;
	};
};

template<typename Alloc>
class BindSettingsGroup : public SettingsGroupBase<BindAllocator<Alloc, false>>
{
	using base_type = SettingsGroupBase<BindAllocator<Alloc, false>>;
	using bind_alloc = BindAllocator<Alloc, false>;
  public:
	using pointer = typename base_type::pointer;
	using size_type = typename base_type::size_type;

	static constexpr unsigned char HEADER_MAGIC = 0xae;
	//If the bind is not active, return false so SettingsChain falls to the next option. If bind group is the last in the chain, the program will crash - as intended
	template<typename T>
	inline bool IsBlocked(T ptr)
	{
	    auto rptr = (typename bind_alloc::real_pointer)((uintptr_t)ptr - bind_alloc::bindOffset);
		return !rptr->bindActive;
	}

	template<typename T, template<typename F> typename U>
	inline void ActivateBind(U<T> ptr, bool active)
	{
	    auto rptr = (typename bind_alloc::real_pointer)((uintptr_t)ptr - bind_alloc::bindOffset);
		rptr->bindActive = active;
	}
};

struct AimbotHitbox
{
	int hitbox = -1;
	unsigned char mask = 0;
	float pointScale = 0.5f;
};

struct SettingsInstance
{
	void* alloc;
	fileHandle fd;
	bool initialized;

	SettingsInstance();
	~SettingsInstance();
};

namespace Settings
{
	extern uintptr_t allocBase;
	extern generic_free_list_allocator<allocBase>* settingsAlloc;

	extern uintptr_t localAllocBase;
	extern generic_free_list_allocator<localAllocBase> settingsLocalAlloc;

	template<typename T>
	using SHMemPtr = typename std::decay<decltype(*Settings::settingsAlloc)>::type::pointer_t<T>;

	template<typename T>
	using LocalOffPtr = typename std::decay<decltype(Settings::settingsLocalAlloc)>::type::pointer_t<T>;

	extern SettingsGroupBase<stateful_allocator<unsigned char, settingsAlloc>>* globalSettingsPtr;
	extern pointer_proxy<globalSettingsPtr> globalSettings;
	extern BindSettingsGroup<stateful_allocator<unsigned char, settingsAlloc>>* bindSettingsPtr;
	extern pointer_proxy<bindSettingsPtr> bindSettings;
	extern SharedMutex* ipcLock;

	extern AimbotHitbox aimbotHitboxes[MAX_HITBOXES];

	extern SettingsInstance sharedInstance;

#define HANDLE_OPTION(...) 1 +
	static constexpr int optionCount =
#include "../bits/option_list.h"
		0;

#define HANDLE_OPTION(type, defaultVal, name, description, ...) extern OPTION(type, name, __VA_ARGS__);
#include "../bits/option_list.h"
}

#endif
