#ifndef PLATFORM_HOOKS_H
#define PLATFORM_HOOKS_H

class AtomicLock;

#ifdef __posix__
union SDL_Event;
struct SDL_Window;
extern uintptr_t origPollEvent;
extern uintptr_t* pollEventJump;
extern uintptr_t origSwapWindow;
extern uintptr_t* swapWindowJump;
#else
extern VFuncHook* hookD3D;

struct IDirect3DDevice9;
extern HWND dxTargetWindow;
extern LONG_PTR oldWndProc;
extern IDirect3DDevice9* d3dDevice;
struct _D3DPRESENT_PARAMETERS_;
#endif

namespace PlatformHooks
{
	extern AtomicLock hookLock;
#ifdef __posix__
	int PollEvent(SDL_Event* event);
	void SwapWindow(struct SDL_Window* window);
#else
	LRESULT __stdcall WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT __stdcall Present(IDirect3DDevice9* device, const RECT* sourceRect, const RECT* destRect, HWND destWindowOverride, const RGNDATA* dirtyRegion);
	LRESULT __stdcall Reset(IDirect3DDevice9* device, struct _D3DPRESENT_PARAMETERS_* presentationParameters);
#endif
}

#endif
