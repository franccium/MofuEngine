#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12 {
class DescriptorHeap;
class ConstantBuffer;
}

namespace mofu::graphics::d3d12::core {

bool Initialize();
void Shutdown();

namespace detail {
void DeferredRelease(IUnknown* resource);
}

template<typename T>
constexpr void Release(T*& resource)
{
	if (resource)
	{
		resource->Release();
		resource = nullptr;
	}
}

template<typename T>
constexpr void DeferredRelease(T*& resource)
{
	if (resource)
	{
		detail::DeferredRelease(resource);
		resource = nullptr;
	}
}

[[nodiscard]] DXDevice* const Device();

[[nodiscard]] DescriptorHeap& RtvHeap();
[[nodiscard]] DescriptorHeap& DsvHeap();
[[nodiscard]] DescriptorHeap& SrvHeap();
[[nodiscard]] DescriptorHeap& UavHeap();
void SetHasDeferredReleases();

[[nodiscard]] ConstantBuffer& CBuffer();

[[nodiscard]] u32 CurrentFrameIndex();


Surface CreateSurface(platform::Window window);
void RemoveSurface(surface_id id);
void RenderSurface(surface_id id, FrameInfo frameInfo);
void ResizeSurface(surface_id id, u32 width, u32 height);
u32 SurfaceWidth(surface_id id);
u32 SurfaceHeight(surface_id id);
}