#include "platform_hooks.h"
#include "fw_bridge.h"
#include "settings.h"
#include "binds.h"
#include "shmfs.h"

#include "../sdk/framework/utils/atomic_lock.h"

#include "../gui/menu/imgui/imgui.h"
#include "../gui/menu/menu.h"
#include "../gui/menu/menuimpl.h"

#ifdef __posix__
#include <GL/gl.h>
#include "../gui/menu/sdlmenu.h"
#include "../gui/menu/imgui/examples/imgui_impl_sdl.h"
#include "../gui/menu/imgui/examples/imgui_impl_opengl2.h"
#else
#include "../gui/menu/dx9menu.h"
//TODO: Abstract ImGui related functions inside DX9Menu
#include "../gui/menu/imgui/examples/imgui_impl_win32.h"
#include "../gui/menu/imgui/examples/imgui_impl_dx9.h"
#endif

AtomicLock PlatformHooks::hookLock;

static bool menuInitialized = false;

extern bool shuttingDown;

static ConVar* cl_mouseenable = nullptr;
static bool prevShown = false;

static void SetupFrame()
{
	//We do not pass-through input at the moment. Also, we can easily do this inside the input handler (filter out mouse moves)
	/*if (!cl_mouseenable)
	  cl_mouseenable = cvar->FindVar(ST("cl_mouseenable"));

	  if (Settings::showMenu != prevShown && cl_mouseenable)
	  cl_mouseenable->SetValue(!prevShown);
	*/

	prevShown = Settings::showMenu;

	ImGuiIO& io = ImGui::GetIO();
	io.MouseDrawCursor = prevShown;

	if (prevShown)
		MenuImpl::Render();
}

static void SetupMenu()
{
	Menu::InitializeStyle();

	ImGuiIO& io = ImGui::GetIO();
	ImFont* font = io.Fonts->AddFontFromMemoryCompressedBase85TTF((const char*)SHMFS::sharedInstance->FindEntry("MenuFont"_crc32)->buffer, 40.f);
	IM_ASSERT(font != NULL);
	font->Scale *= 0.5f;

	io.IniFilename = nullptr;
}

#ifdef __posix__
uintptr_t origPollEvent = 0;
uintptr_t* pollEventJump = nullptr;

uintptr_t origSwapWindow = 0;
uintptr_t* swapWindowJump = nullptr;

SDL_GLContext imguiContext = nullptr;
SDL_Window* lastWindow = nullptr;

static constexpr uint32_t filteredMessages[] = {
	SDL_KEYDOWN,
	SDL_FINGERDOWN,
	SDL_MOUSEBUTTONDOWN,
	SDL_MOUSEMOTION,
	SDL_MOUSEWHEEL,
	SDL_TEXTINPUT,
	SDL_TEXTEDITING
};

static bool IsEventUnfiltered(uint32_t event)
{
	for (uint32_t i : filteredMessages)
		if (i == event)
			return false;

	return true;
}

int PlatformHooks::PollEvent(SDL_Event* event)
{
	auto OrigSDL_PollEvent = (decltype(PlatformHooks::PollEvent)*)origPollEvent;

	int ret = OrigSDL_PollEvent(event);

	if (!ret)
		return 0;

	if (event->type == SDL_KEYUP)
		BindManager::sharedInstance->binds[event->key.keysym.scancode].HandleKeyPress(false);
	else if (event->type == SDL_KEYDOWN)
		BindManager::sharedInstance->binds[event->key.keysym.scancode].HandleKeyPress(true);

	if (imguiContext && Settings::showMenu) {
		SDLMenu::PollEvent(event);
		if (!IsEventUnfiltered(event->type))
			return PollEvent(event);
	}

	return ret;
}

static void InitializeMenu(SDL_Window* window)
{
	if (menuInitialized)
		return;

	ImGui::CreateContext();
	SDLMenu::InitializeContext(imguiContext, window);

	SetupMenu();

	menuInitialized = true;
}

void PlatformHooks::SwapWindow(SDL_Window* window)
{
	auto origFn = (decltype(PlatformHooks::SwapWindow)*)origSwapWindow;
	SDL_GLContext originalContext = SDL_GL_GetCurrentContext();

	lastWindow = window;

	if (PlatformHooks::hookLock.trylock()) {
		if (!shuttingDown) {
			if (!imguiContext)
				imguiContext = SDL_GL_CreateContext(window);

			InitializeMenu(window);

			SDL_GL_MakeCurrent(window, imguiContext);

			SDLMenu::NewFrame(window);

			SetupFrame();

			ImGui::EndFrame();
			ImGui::Render();
			ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

			//#ifdef __linux__
			origFn(window);
			SDL_GL_MakeCurrent(window, originalContext);
			glFlush();
		}
		PlatformHooks::hookLock.unlock();
	} else
		origFn(window);
}

#else
HWND dxTargetWindow;
LONG_PTR oldWndProc = 0;
IDirect3DDevice9* d3dDevice = nullptr;

static void InitializeMenu(IDirect3DDevice9* device)
{
	if (menuInitialized)
		return;

	D3DDEVICE_CREATION_PARAMETERS params;

	if (FAILED(device->GetCreationParameters(&params)))
		return;

	DX9Menu::InitializeContext(d3dDevice, params.hFocusWindow);

	SetupMenu();

	menuInitialized = true;
}

static constexpr UINT filteredMessages[] = {
	WM_KEYDOWN,
	WM_SYSKEYDOWN,
	WM_NCLBUTTONDOWN,
	WM_NCRBUTTONDOWN,
	WM_NCMBUTTONDOWN,
	WM_LBUTTONDOWN,
	WM_RBUTTONDOWN,
	WM_MBUTTONDOWN
};

static bool IsEventUnfiltered(UINT msg)
{
	for (UINT i : filteredMessages)
		if (i == msg)
			return false;

	return true;
}

LRESULT __stdcall PlatformHooks::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (Settings::showMenu && menuInitialized && DX9Menu::WndProc(hWnd, msg, wParam, lParam))
		return true;

	if (wParam >= 0 && wParam < 256) {
		if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
			BindManager::sharedInstance->binds[WIN_NATIVE_TO_HID[wParam]].HandleKeyPress(false);
		else if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
			BindManager::sharedInstance->binds[WIN_NATIVE_TO_HID[wParam]].HandleKeyPress(true);
	}

	//TODO: Input pass-through option
	if (!Settings::showMenu || IsEventUnfiltered(msg))
		return CallWindowProcW((WNDPROC)oldWndProc, hWnd, msg, wParam, lParam);

	return true;
}

LRESULT __stdcall PlatformHooks::Present(IDirect3DDevice9* device, const RECT* sourceRect, const RECT* destRect, HWND destWindowOverride, const RGNDATA* dirtyRegion)
{
	static auto origFn = hookD3D->GetOriginal(PlatformHooks::Present);

	if (PlatformHooks::hookLock.trylock()) {
		if (!shuttingDown) {
			InitializeMenu(device);

			unsigned long colorWrite, srgbWrite;

			device->GetRenderState(D3DRS_COLORWRITEENABLE, &colorWrite);
			device->GetRenderState(D3DRS_SRGBWRITEENABLE, &srgbWrite);
			device->SetRenderState(D3DRS_COLORWRITEENABLE, ~0u);
			device->SetRenderState(D3DRS_SRGBWRITEENABLE, false);

			ImGui_ImplDX9_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			SetupFrame();

			ImGui::EndFrame();
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

			device->SetRenderState(D3DRS_COLORWRITEENABLE, colorWrite);
			device->SetRenderState(D3DRS_SRGBWRITEENABLE, srgbWrite);
		}
		PlatformHooks::hookLock.unlock();
	}

	return origFn(device, sourceRect, destRect, destWindowOverride, dirtyRegion);
}

LRESULT __stdcall PlatformHooks::Reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* presentationParameters)
{
	static auto origFn = hookD3D->GetOriginal(PlatformHooks::Reset);

	PlatformHooks::hookLock.lock();

	if (shuttingDown) {
		PlatformHooks::hookLock.unlock();
		return origFn(device, presentationParameters);
	}

	InitializeMenu(device);

	ImGui_ImplDX9_InvalidateDeviceObjects();
	LRESULT ret = origFn(device, presentationParameters);
	ImGui_ImplDX9_CreateDeviceObjects();

	PlatformHooks::hookLock.unlock();

	return ret;
}

#endif
