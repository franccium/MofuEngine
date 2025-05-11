#include "D3D12Helpers.h"

namespace mofu::graphics::d3d12::d3dx {
   
// if the initial data is going to be changed later, we need it to be cpu-accessible,
// if we only want to upload some data to be used by the GPU, isCpuAccessible should be set to false
DXResource*
CreateResourceBuffer(const void* data, u32 bufferSize, bool isCpuAccessible /* false */,
	D3D12_RESOURCE_STATES state /* D3D12_RESOURCE_STATE_COMMON */,
	D3D12_RESOURCE_FLAGS flags /* NONE */, DXHeap* heap /* nullptr */, u64 heapOffset /* 0 */)
{
	assert(bufferSize != 0);

	D3D12_RESOURCE_DESC1 desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.Width = bufferSize;
	desc.Height = 1;
	desc.Alignment = 0;
	desc.MipLevels = 1;
	desc.DepthOrArraySize = 1;
	desc.SampleDesc = { 1,0 };
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = isCpuAccessible ? D3D12_RESOURCE_FLAG_NONE : flags;

	// the buffer will only be used for upload or as a constant buffer/UAV
	assert(desc.Flags == D3D12_RESOURCE_FLAG_NONE || D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	DXResource* resource{ nullptr };
	const D3D12_RESOURCE_STATES resourceState{ isCpuAccessible ? D3D12_RESOURCE_STATE_GENERIC_READ : state };

	if (heap)
	{
		DXCall(core::Device()->CreatePlacedResource1(heap, heapOffset, &desc, resourceState, nullptr, IID_PPV_ARGS(&resource)));
	}
	else
	{
		DXCall(core::Device()->CreateCommittedResource2(isCpuAccessible ? &HeapProperties.UploadHeap : &HeapProperties.DefaultHeap,
			D3D12_HEAP_FLAG_CREATE_NOT_ZEROED, &desc, resourceState, nullptr, nullptr, IID_PPV_ARGS(&resource)));
	}

	if (data)
	{
		if (isCpuAccessible)
		{
			// NOTE: range begin and end are set to 0 to indicate that the CPU is not reading any data (write-only)
			const D3D12_RANGE range{};
			void* cpuAddress{ nullptr };
			DXCall(resource->Map(0, &range, reinterpret_cast<void**>(&cpuAddress)));
			assert(cpuAddress);
			memcpy(cpuAddress, data, bufferSize);
			resource->Unmap(0, nullptr);
		}
		else
		{
			// we need an upload context that creates an upload resource and copies it to the GPU-only resource
			// TODO: create an upload context
		}
	}

	assert(resource);
	return resource;
}

}

