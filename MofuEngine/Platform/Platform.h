#pragma once
#include "CommonHeaders.h"
#include "Window.h"

#ifdef _WIN64
#define INCLUDE_WIN_PLATFORM_CODE
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
	i32 left{ 0 };
	i32 top{ 0 };
	i32 width{ 1920 };
	i32 height{ 1080 };
};

// CreateWindow was taken by win32
Window ConcoctWindow(const WindowInitInfo* const initInfo = nullptr);
void RemoveWindow(window_id id);
}