#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::d3dx {

constexpr struct
{
    const D3D12_HEAP_PROPERTIES DefaultHeap
    {
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0
    };

    const D3D12_HEAP_PROPERTIES UploadHeap
    {
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0
    };
} HeapProperties;

DXResource* CreateResourceBuffer(const void* data, u32 bufferSize,
    bool isCpuAccessible = false,
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON,
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
    DXHeap* heap = nullptr, u64 heapOffset = 0);

// constant buffer data has to be aligned to a multiple of 256 bytes
constexpr u64 
AlignSizeForConstantBuffer(u64 size)
{
	return math::AlignUp<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(size);
}

}