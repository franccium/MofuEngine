#pragma once
#include "D3D12CommonHeaders.h"
#include "D3D12Core.h"

// Provides utility classes for D3D12 resource management

namespace mofu::graphics::d3d12 {
struct DescriptorHandle
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
	D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
	u32 index{ U32_INVALID_ID };

	[[nodiscard]] constexpr bool IsValid() const { return cpu.ptr != 0; }
	[[nodiscard]] constexpr bool IsShaderVisible() const { return gpu.ptr != 0; }

#ifdef _DEBUG
private:
	friend class DescriptorHeap;
	DescriptorHeap* container{ nullptr };
#endif
};



}