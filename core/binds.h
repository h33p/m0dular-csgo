#ifndef BINDS_H
#define BINDS_H

#include "../sdk/framework/utils/settings.h"
#include "../sdk/framework/utils/freelistallocator.h"
#include "../sdk/framework/utils/allocwraps.h"
#include "settings.h"
#include <vector>

//TODO: Remove the unnecessary pointer type templating since now we use shmemptr even in the interface level

enum BindMode : unsigned char
{
    HOLD = 0,
	TOGGLE
};

//All these types are a bit dependant on each other, but only the behavior, not the data
struct BindDataIFace;
struct BindHandlerIFace;

struct BindHandlerIFaceVtable
{
	//We can not really be non-virtual but we also can not have virtual method tables stored in shared memory. So, this is the manual vtable
	BindDataIFace* (*AllocKeyBind)(BindHandlerIFace*) = nullptr;
	void (*ReleaseKeyBind)(BindHandlerIFace*, BindDataIFace*) = nullptr;
	void (*HandleEnable)(BindHandlerIFace*, BindDataIFace*) = nullptr;
	void (*HandleDisable)(BindHandlerIFace*, BindDataIFace*) = nullptr;
};

struct BindHandlerIFace
{
	Settings::LocalOffPtr<BindHandlerIFaceVtable> vtbl;

	BindDataIFace* AllocKeyBind()
	{
		return vtbl->AllocKeyBind(this);
	}

	inline void ReleaseKeyBind(BindDataIFace* data)
	{
		vtbl->ReleaseKeyBind(this, data);
	}

	inline void HandleEnable(BindDataIFace* data)
	{
		vtbl->HandleEnable(this, data);
	}

	inline void HandleDisable(BindDataIFace* data)
	{
		vtbl->HandleDisable(this, data);
	}

};

struct BindDataIFace
{
	//TODO: fill this with functions
	Settings::SHMemPtr<BindHandlerIFace> handler;

	BindDataIFace(BindHandlerIFace* hptr)
		: handler(hptr)
	{
	}

	~BindDataIFace()
	{
		handler->ReleaseKeyBind(this);
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

	template<typename Alloc, typename T>
	BindHandlerBase(BindSettingsGroup<Alloc>* bindAlloc, crcs_t crc, const T& val)
	{
		pointer = (Ptr)bindAlloc->ReserveOption(crc, val);
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
	using ThisType = BindHandlerImpl<T, SettingsGroup, Ptr, Alloc>;
	Alloc alloc;
	using TypePointer = typename std::pointer_traits<Ptr>::template rebind<T>;
	using TrueBindPointer = BindDataImpl<T>*;
	using BindPointer = typename std::pointer_traits<Ptr>::template rebind<BindDataImpl<T>>;
	using VecAlloc = typename Alloc::template rebind<BindPointer>::other;

	std::vector<BindPointer, VecAlloc> activeBinds;

	template<typename... Args>
	BindHandlerImpl(Args... args)
		: BindHandlerBase<Ptr>(args...)
	{
	}

	inline void InitializeVTable()
	{
		Base::vtbl = Settings::settingsLocalAlloc.allocate<BindHandlerIFaceVtable>(1);
		Base::vtbl->AllocKeyBind = AllocKeyBindST;
		Base::vtbl->ReleaseKeyBind = ReleaseKeyBindST;
		Base::vtbl->HandleEnable = HandleEnableST;
		Base::vtbl->HandleDisable = HandleDisableST;
	}

	BindDataIFace* AllocKeyBind()
	{
		auto ret = alloc.allocate(1);
		ret = new(ret) BindDataImpl<T>(this);
		return (BindDataIFace*)ret;
	}

	static inline BindDataIFace* AllocKeyBindST(BindHandlerIFace* iface)
	{
		return ((ThisType*)iface)->AllocKeyBind();
	}

	void ReleaseKeyBind(BindDataIFace* keyBindIFace)
	{
		auto keyBind = (BindPointer)(TrueBindPointer)keyBindIFace;
		HandleDisable(keyBindIFace);
		alloc.deallocate(keyBind, 1);
	}

	static inline void ReleaseKeyBindST(BindHandlerIFace* iface, BindDataIFace* keyBindIFace)
	{
	    ((ThisType*)iface)->ReleaseKeyBind(keyBindIFace);
	}

	void HandleEnable(BindDataIFace* keyBindIFace)
	{
		auto keyBind = (BindPointer)(TrueBindPointer)keyBindIFace;

		//Delete any previous instances just in case...
		HandleDisable(keyBindIFace);

		activeBinds.push_back(keyBind);

		*(TypePointer)Base::pointer = keyBind->value;
	    SettingsGroup->ActivateBind(Base::pointer, true);
	}

	static inline void HandleEnableST(BindHandlerIFace* iface, BindDataIFace* keyBindIFace)
	{
	    ((ThisType*)iface)->HandleEnable(keyBindIFace);
	}

	void HandleDisable(BindDataIFace* keyBindIFace)
	{
		auto keyBind = (BindPointer)(TrueBindPointer)keyBindIFace;

		for (auto& i : activeBinds)
			if (i == keyBind)
				i = nullptr;

		//Remove all null pointers from the top of the stack
		while (activeBinds.size() && !activeBinds.back())
			activeBinds.pop_back();

		if (activeBinds.size()) {
			*(TypePointer)Base::pointer = activeBinds.back()->value;
			SettingsGroup->ActivateBind(Base::pointer, true);
		} else
			SettingsGroup->ActivateBind(Base::pointer, false);
	}

	static inline void HandleDisableST(BindHandlerIFace* iface, BindDataIFace* keyBindIFace)
	{
	    ((ThisType*)iface)->HandleDisable(keyBindIFace);
	}
};

struct BindKey
{
	BindMode mode;
	char state;
	bool down;
	Settings::SHMemPtr<BindDataIFace> pointer;

	BindKey(BindDataIFace* p = nullptr, BindMode bMode = BindMode::HOLD)
		: mode(bMode), state(0), pointer(p)
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

	template<typename T>
	inline void BindPointer(BindHandlerIFace* handler, const T& value)
	{
		if (pointer)
		    pointer->~BindDataIFace();
		pointer = handler->AllocKeyBind();
		((BindDataImpl<T>*)&*pointer)->value = value;
	}

	template<typename T>
	inline void InitializePointer(const T& value)
	{
		if (pointer)
			((BindDataImpl<T>*)&*pointer)->value = value;
	}

};

using BindSettingsType = typename std::decay<decltype(*Settings::bindSettingsPtr)>::type;

template<typename T>
using BindImpl = BindHandlerImpl<T, Settings::bindSettings, BindSettingsType::pointer, stateful_allocator<BindDataImpl<T>, Settings::settingsAlloc>>;

struct BindManagerInstance
{
	using HandlerPointer = Settings::SHMemPtr<BindHandlerIFace>;
	HandlerPointer bindList[Settings::optionCount];
	BindKey binds[256];

	BindManagerInstance();
	~BindManagerInstance();
	void InitializeLocalData();
};

namespace BindManager
{
	extern BindManagerInstance* sharedInstance;
	std::vector<unsigned char> SerializeBinds();
	void LoadBinds(const std::vector<unsigned char>& vec);
}

#endif
