#pragma once
#include "CommonHeaders.h"
#include "Window.h"

#ifdef _WIN64
#include "PlatformWin32.h"
#elif
// ...
#endif

namespace mofu::platform {
struct WindowInitInfo 
{
	WindowEventHandler windowEventCallback{ nullptr };
	WindowHandle parentWindow{ nullptr };
	const wchar_t* caption{ nullptr };
	s32 left{ 0 };
	s32 top{ 0 };
	s32 width{ 1920 };
	s32 height{ 1080 };
};

// CreateWindow was taken by win32
Window ConcoctWindow(const WindowInitInfo* const initInfo = nullptr);
void RemoveWindow(window_id id);
}