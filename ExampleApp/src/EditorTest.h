#pragma once
#include "Platform/Platform.h"
#include "Graphics/Renderer.h"
#include "Content/ShaderCompilation.h"
#include <thread>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "Core/EngineModules.h"
#include "ECS/Entity.h"
#include "Graphics/Renderer.h"
#include "Graphics/UIRenderer.h"
#include "EngineAPI/Camera.h"
#include "Content/ResourceCreation.h"
#include "Graphics/GeometryData.h"
#include "TestTimer.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "Editor/Project/Project.h"
#include "Editor/EditorInterface.h"

#include "tracy/Tracy.hpp"

constexpr u32 WINDOW_COUNT{ 1 };

constexpr const char* TEST_PROJECT_PATH{ "Projects/TestProject/" };
constexpr const char* TEST_PROJECT_FILE_PATH{ "Projects/TestProject/TestProject.mpj" };

using namespace mofu;

struct CameraSurface
{
	graphics::RenderSurface surface{};
	ecs::Entity entity{};
	graphics::Camera camera{};
};
CameraSurface renderSurfaces[WINDOW_COUNT]{};

bool isRunning{ true };
bool isRestarting{ false };
bool isResized{ false };

u32 renderItemCount{ 0 };
Vec<id_t> renderItemIDsCache{};

Timer timer{};

bool MofuInitialize();
void MofuShutdown();
void InitializeRenderingTest();
void ShutdownRenderingTest();
u32 CreateTestRenderItems();
void GetRenderItemIDS(Vec<id_t>& outIds);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (!isRunning) return DefWindowProc(hwnd, msg, wparam, lparam);

	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) return true;

	bool toggleFullscreen{ false };
	switch (msg)
	{
	case WM_DESTROY:
	{
		bool allWindowsClosed{ true };
		for (u32 i = 0; i < WINDOW_COUNT; ++i)
		{
			graphics::RenderSurface& surface{ renderSurfaces[i].surface };
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
			if (win.GetID() == renderSurfaces[i].surface.window.GetID())
			{
				if (toggleFullscreen)
				{
					win.SetFullscreen(!win.IsFullscreen());
					return 0;
				}
				else
				{
					renderSurfaces[i].surface.surface.Resize(win.Width(), win.Height());
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
	mofu::InitializeEngineModules();
	while (!CompileEngineShaders())
	{
		if (MessageBox(nullptr, L"Failed to compile engine shaders", L"Error", MB_RETRYCANCEL) != IDRETRY) return false;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::StyleColorsDark();

	if (!graphics::Initialize(graphics::GraphicsPlatform::Direct3D12)) return false;

	editor::project::LoadProject(TEST_PROJECT_FILE_PATH);

	if (!editor::InitializeEditorGUI()) return false;

	platform::WindowInitInfo info[]
	{
		{&WindowProc, nullptr, L"MofuEngine", 100, 50, 1600, 900},
		//{&WindowProc, nullptr, L"TestW2", 600, 100, 800, 600},
		//{&WindowProc, nullptr, L"TestW3", 300, 600, 200, 600},
		//{&WindowProc, nullptr, L"TestW4", 400, 800, 1300, 200},
	};
	static_assert(_countof(info) == _countof(renderSurfaces));

	for (u32 i = 0; i < WINDOW_COUNT; ++i)
	{
		CameraSurface& cSurf{ renderSurfaces[i] };
		cSurf.surface.window = platform::ConcoctWindow(&info[i]);
		cSurf.surface.surface = graphics::CreateSurface(cSurf.surface.window);

		v3 pos{ 0.f, 0.f, 15.f };
		quat rot{ quatIndentity };
		v3 scale{ 1.f, 1.f, 1.f };
		ecs::component::LocalTransform lt{ {}, pos, rot, scale };
		ecs::component::Camera cam{};
		ecs::EntityData& entityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::Camera>(lt, cam) };
		cSurf.entity = entityData.id;

		cSurf.camera = graphics::CreateCamera(graphics::PerspectiveCameraInitInfo{ cSurf.entity });
		cSurf.camera.AspectRatio((f32)cSurf.surface.window.Width() / cSurf.surface.window.Height());
	}

	ImGui_ImplWin32_Init(renderSurfaces[0].surface.window.Handle());

	InitializeRenderingTest();

	renderItemCount = CreateTestRenderItems();
	renderItemIDsCache.resize(renderItemCount);
	//!!! content::GetRenderItemIDs(renderItemIDsCache.data(), renderItemCount);
	GetRenderItemIDS(renderItemIDsCache);

	editor::project::RefreshAllAssets();

	return true;
}

void MofuUpdate()
{
	ZoneScoped;

	timer.Start();

	{
		ZoneScopedN("ECS update");
		ecs::system::SystemUpdateData ecsUpdateData{};
		ecsUpdateData.DeltaTime = 16.7f;
		ecs::Update(ecsUpdateData);
	}

	graphics::ui::StartNewFrame();

	for (u32 i{ 0 }; i < WINDOW_COUNT; ++i)
	{
		if (renderSurfaces[i].surface.surface.IsValid())
		{
			//frameInfo.LastFrameTime = 16.7f;
			//frameInfo.AverageFrameTime = 16.7f;
			//frameInfo.RenderItemCount = renderItemCount;
			//frameInfo.CameraID = renderSurfaces[i].camera.GetID();
			//frameInfo.Thresholds = thresholds.data();
			//frameInfo.RenderItemIDs = renderItemIDsCache.data();

			renderSurfaces[i].surface.surface.Render(graphics::GetCurrentFrameInfo());
		}
	}
	timer.End();

	FrameMark;
}

void MofuShutdown()
{
	if (!isRunning) return;

	editor::project::UnloadProject();
	//graphics::ui::Shutdown();
	ShutdownRenderingTest();

	for (u32 i = 0; i < WINDOW_COUNT; ++i)
	{
		CameraSurface& cSurf{ renderSurfaces[i] };
		if (cSurf.surface.surface.IsValid()) graphics::RemoveSurface(cSurf.surface.surface.GetID());
		if (cSurf.surface.window.IsValid()) platform::RemoveWindow(cSurf.surface.window.GetID());
		if (cSurf.camera.IsValid()) graphics::RemoveCamera(cSurf.camera.GetID());
	}
	graphics::Shutdown();

	isRunning = false;
}