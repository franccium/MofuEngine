#pragma once
#include "Platform/Platform.h"
#include <thread>

#define WINDOW_COUNT 1

using namespace mofu;

platform::Window _windows[WINDOW_COUNT];

LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_DESTROY:
	{
		bool all_windows_closed{ true };
		for (u32 i = 0; i < WINDOW_COUNT; ++i)
		{
			if (!_windows[i].IsClosed())
			{
				all_windows_closed = false;
			}
		}
		if (all_windows_closed)
		{
			PostQuitMessage(0);
			return 0;
		}
		break;
	}
	case WM_SYSCHAR:
		if (wparam == VK_RETURN && (HIWORD(lparam) & KF_ALTDOWN))
		{
			platform::Window win{ platform::window_id( (id_t)GetWindowLongPtr(hwnd, GWLP_USERDATA)) };
			win.SetFullscreen(!win.IsFullscreen());
			return 0;
		}
		break;
	default:
		break;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

bool MofuInitialize()
{
	platform::WindowInitInfo info[]
	{
		{&WindowProc, nullptr, L"TestW1", 200, 200, 400, 600},
	};
	static_assert(_countof(info) == _countof(_windows));

	for (u32 i = 0; i < WINDOW_COUNT; ++i)
	{
		_windows[i] = platform::ConcoctWindow(&info[i]);
	}
	return true;
}

void MofuUpdate()
{
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void MofuShutdown()
{
	for (u32 i = 0; i < WINDOW_COUNT; ++i)
	{
		platform::RemoveWindow(_windows[i].GetID());
	}
}