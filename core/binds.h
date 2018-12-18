#ifndef BINDS_H
#define BINDS_H

#include "../sdk/framework/utils/settings.h"
#include "../sdk/framework/utils/freelistallocator.h"
#include "../sdk/framework/utils/allocwraps.h"
#include "settings.h"
#include <vector>

enum BindMode : unsigned char
{
    HOLD = 0,
	TOGGLE
};

//All these types are a bit dependant on each other, but only the behavior, not the data
struct BindDataIFace;

struct BindHandlerIFace
{
	//There is not really a simple non-virtual way to handle different data types without holding pointers to templated functions anyways
	virtual BindDataIFace* AllocKeyBind() = 0;
	virtual void HandleEnable(const BindDataIFace*) = 0;
	virtual void HandleDisable(const BindDataIFace*) = 0;
};

struct BindDataIFace
{
	BindHandlerIFace* handler;

	BindDataIFace(BindHandlerIFace* hptr)
		: handler(hptr)
	{
	}

	virtual ~BindDataIFace()
	{
		handler->HandleDisable(this);
	}

	void HandleEnable()
	{
		handler->HandleEnable(this);
	}

	void HandleDisable()
	{
		handler->HandleDisable(this);
	}
};

template<typename Ptr>
struct BindHandlerBase : BindHandlerIFace
{
	Ptr pointer;

	BindHandlerBase()
		: pointer(nullptr)
	{
	}

	BindHandlerBase(Ptr ptr)
		: pointer(ptr)
	{
	}

	template<typename Alloc>
	BindHandlerBase(BindSettingsGroup<Alloc>* bindAlloc, crcs_t crc)
	{
		pointer = (Ptr)bindAlloc->TryGetAlloc(crc);
	}
};

template<typename T>
struct BindDataImpl : BindDataIFace
{
	T value;

	BindDataImpl(BindHandlerIFace* handler)
		: BindDataIFace::BindDataIFace(handler), value()
	{
	}
};

template<typename T, auto& SettingsGroup, typename Ptr, class Alloc = std::allocator<BindDataImpl<T>>>
struct BindHandlerImpl : BindHandlerBase<Ptr>
{
	using Base = BindHandlerBase<Ptr>;
	Alloc alloc;
	using VecAlloc = typename Alloc::template rebind<const BindDataImpl<T>*>::other;
	using TypePointer = typename std::pointer_traits<Ptr>::template rebind<T>;

	std::vector<const BindDataImpl<T>*, VecAlloc> activeBinds;

	template<typename... Args>
	BindHandlerImpl(Args... args)
		: BindHandlerBase<Ptr>(args...)
	{
	}

	virtual BindDataIFace* AllocKeyBind() override
	{
		auto ret = alloc.allocate(1);
		return (BindDataIFace*)ret;
	}

	virtual void HandleEnable(const BindDataIFace* keyBindIFace) override
	{
	    auto keyBind = (const BindDataImpl<T>*)keyBindIFace;

		//Delete any previous instances just in case...
		HandleDisable(keyBindIFace);

		activeBinds.push_back(keyBind);

		*(TypePointer)Base::pointer = keyBind->value;
	    SettingsGroup->ActivateBind(Base::pointer, true);
	}

	virtual void HandleDisable(const BindDataIFace* keyBindIFace) override
	{
	    auto keyBind = (const BindDataImpl<T>*)keyBindIFace;

		for (auto& i : activeBinds)
			if (i == keyBind)
				i = nullptr;

		//Remove all null pointers from the top of the stack
		while (activeBinds.size() && activeBinds.back())
			activeBinds.pop_back();

		if (activeBinds.size()) {
			*(TypePointer)Base::pointer = activeBinds.back()->value;
			SettingsGroup->ActivateBind(Base::pointer, true);
		} else
			SettingsGroup->ActivateBind(Base::pointer, false);
	}
};

struct BindKey
{
	using BindDataPtr = void*;

	BindMode mode;
	char state;
	bool down;
	BindDataPtr pointer;

	BindKey()
		: mode(BindMode::HOLD), state(0), pointer(nullptr)
	{
	}

	inline void Unserialize(std::vector<unsigned char>& vec, size_t& i)
	{
	    mode = (BindMode)vec[i++];
		state = 0;
	}

	inline void Serialize(std::vector<unsigned char>& vec)
	{
		vec.push_back((unsigned char)mode);
	}

	inline void HandleKeyPress(bool isDown)
	{
		if (down == isDown)
			return;

		down = isDown;

		switch(mode) {
		  case BindMode::HOLD:
			  break;
		  case BindMode::TOGGLE:
			  break;
		}
	}
};

using BindSettingsType = typename std::decay<decltype(*Settings::bindSettingsPtr)>::type;

template<typename T>
using BindImpl = BindHandlerImpl<T, Settings::bindSettings, BindSettingsType::pointer, stateful_allocator<BindDataImpl<T>, Settings::settingsAlloc>>;

namespace BindManager
{
	extern BindHandlerIFace* bindList[];

	std::vector<unsigned char> SerializeBinds();
	void LoadBinds(const std::vector<unsigned char>& vec);
}

#endif
