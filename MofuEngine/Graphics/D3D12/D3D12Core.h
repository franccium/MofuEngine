#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::core {
bool Initialize();
void Shutdown();

template<typename T>
constexpr void Release(T*& resource)
{
	if (resource)
	{
		resource->Release();
		resource = nullptr;
	}
}

Surface CreateSurface(platform::Window window);
void RemoveSurface(surface_id id);
void RenderSurface(surface_id id, FrameInfo frameInfo);
void ResizeSurface(surface_id id, u32 width, u32 height);
u32 SurfaceWidth(surface_id id);
u32 SurfaceHeight(surface_id id);
}