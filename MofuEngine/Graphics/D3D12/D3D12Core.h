#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12 {
class DescriptorHeap;
class ConstantBuffer;
namespace camera { class D3D12Camera; }

struct D3D12FrameInfo
{
	const FrameInfo* Info{ nullptr };
	camera::D3D12Camera* Camera{ nullptr };
	D3D12_GPU_VIRTUAL_ADDRESS GlobalShaderData{ 0 }; // a pointer to a constant buffer that contains the view and projection matrices, etc.
	u32 SurfaceWidth{ 0 };
	u32 SurfaceHeight{ 0 };
	u32 FrameIndex{ 0 };
	id_t LightCullingID{ 0 };
	f32 DeltaTime{ 16.7f };
};

}

namespace mofu::graphics::d3d12::core {

bool Initialize();
void Shutdown();

void RenderItemsUpdated();

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