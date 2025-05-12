#pragma once
#include "Platform/Platform.h"
#include "Graphics/Renderer.h"
#include "Content/ShaderCompilation.h"
#include <thread>

constexpr u32 WINDOW_COUNT{ 1 };

using namespace mofu;

graphics::RenderSurface renderSurfaces[WINDOW_COUNT];

bool isRunning{ true };
bool isRestarting{ false };
bool isResized{ false };

bool MofuInitialize();
void MofuShutdown();

LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (!isRunning) return DefWindowProc(hwnd, msg, wparam, lparam);

	bool toggleFullscreen{ false };
	switch (msg)
	{
	case WM_DESTROY:
	{
		bool allWindowsClosed{ true };
		for (u32 i = 0; i < WINDOW_COUNT; ++i)
		{
			graphics::RenderSurface& surface{ renderSurfaces[i] };
			if (!surface.window.IsValid()) continue;
			if (surface.window.IsClosed())
			{
				if (surface.surface.IsValid()) graphics::RemoveSurface(surface.surface.GetID());
				if (surface.window.IsValid()) platform::RemoveWindow(surface.window.GetID());
			}
			else
			{
				allWindowsClosed = false;
			}
		}
		if (allWindowsClosed && !isRestarting)
		{
			MofuShutdown();
			PostQuitMessage(0);
			return 0;
		}
		break;
	}
	case WM_SIZE:
		isResized = (wparam != SIZE_MINIMIZED);
		break;
	case WM_SYSCHAR:
		toggleFullscreen = (wparam == VK_RETURN && (HIWORD(lparam) & KF_ALTDOWN));
		break;
	case WM_KEYDOWN:
		if (wparam == VK_ESCAPE)
		{
			PostMessage(hwnd, WM_CLOSE, 0, 0);
			return 0;
		}
		else if (wparam == VK_F11)
		{
			isRestarting = true;
			MofuShutdown();
			MofuInitialize();
		}
		break;
	default:
		break;
	}

	if ((isResized && GetKeyState(VK_LBUTTON) >= 0) || toggleFullscreen)
	{
		platform::Window win{ platform::window_id{ (id_t)GetWindowLongPtr(hwnd, GWLP_USERDATA)} };
		for (u32 i{ 0 }; i < _countof(renderSurfaces); ++i)
		{
			if (win.GetID() == renderSurfaces[i].window.GetID())
			{
				if (toggleFullscreen)
				{
					win.SetFullscreen(!win.IsFullscreen());
					return 0;
				}
				else
				{
					renderSurfaces[i].surface.Resize(win.Width(), win.Height());
					isResized = false;
				}
				break;
			}
		}
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

bool MofuInitialize()
{
	while (!CompileEngineShaders())
	{
		if (MessageBox(nullptr, L"Failed to compile engine shaders", L"Error", MB_RETRYCANCEL) != IDRETRY) return false;
	}

	if(!graphics::Initialize(graphics::GraphicsPlatform::Direct3D12)) return false;

	platform::WindowInitInfo info[]
	{
		{&WindowProc, nullptr, L"TestW1", 200, 200, 400, 600},
		//{&WindowProc, nullptr, L"TestW2", 600, 100, 800, 600},
		//{&WindowProc, nullptr, L"TestW3", 300, 600, 200, 600},
		//{&WindowProc, nullptr, L"TestW4", 400, 800, 1300, 200},
	};
	static_assert(_countof(info) == _countof(renderSurfaces));

	for (u32 i = 0; i < WINDOW_COUNT; ++i)
	{
		renderSurfaces[i].window = platform::ConcoctWindow(&info[i]);
		renderSurfaces[i].surface = graphics::CreateSurface(renderSurfaces[i].window);
	}

	return true;
}

void MofuUpdate()
{
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	graphics::FrameInfo frameInfo{};
	frameInfo.lastFrameTime = 16.7f;
	frameInfo.averageFrameTime = 16.7f;
	for (u32 i{ 0 }; i < WINDOW_COUNT; ++i)
	{
		if (renderSurfaces[i].surface.IsValid())
		{
			renderSurfaces[i].surface.Render(frameInfo);
		}
	}
}

void MofuShutdown()
{
	if (!isRunning) return;

	for (u32 i = 0; i < WINDOW_COUNT; ++i)
	{
		graphics::RenderSurface& surface{ renderSurfaces[i] };
		if (surface.surface.IsValid()) graphics::RemoveSurface(surface.surface.GetID());
		if (surface.window.IsValid()) platform::RemoveWindow(surface.window.GetID());
	}
	graphics::Shutdown();
	isRunning = false;
}