#pragma once

#ifndef WIN32_MEAN_AND_LEAN
#define WIN32_MEAN_AND_LEAN
#include <Windows.h>
#endif

namespace mofu::platform {
using WindowEventHandler = LRESULT(*)(HWND, UINT, WPARAM, LPARAM);
using WindowHandle = HWND;

}