#pragma once
#include "Platform/Platform.h"
#include "Graphics/Renderer.h"
#include <thread>

constexpr u32 WINDOW_COUNT = 1;

using namespace mofu;

platform::Window windows[WINDOW_COUNT];
graphics::Surface renderSurfaces[WINDOW_COUNT];

LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_DESTROY:
	{
		bool all_windows_closed{ true };
		for (u32 i = 0; i < WINDOW_COUNT; ++i)
		{
			if (!windows[i].IsClosed())
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
	// TODO: compile shaders

	if(!graphics::Initialize(graphics::GraphicsPlatform::Direct3D12)) return false;

	platform::WindowInitInfo info[]
	{
		{&WindowProc, nullptr, L"TestW1", 200, 200, 400, 600},
		//{&WindowProc, nullptr, L"TestW2", 600, 100, 800, 600},
		//{&WindowProc, nullptr, L"TestW3", 300, 600, 200, 600},
		//{&WindowProc, nullptr, L"TestW4", 400, 800, 1300, 200},
	};
	static_assert(_countof(info) == _countof(windows));

	for (u32 i = 0; i < WINDOW_COUNT; ++i)
	{
		windows[i] = platform::ConcoctWindow(&info[i]);
		renderSurfaces[i] = graphics::CreateSurface(windows[i]);
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
		platform::RemoveWindow(windows[i].GetID());
	}
}