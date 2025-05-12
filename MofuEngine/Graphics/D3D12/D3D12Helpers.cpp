#include "D3D12Helpers.h"
#include "D3D12Core.h"

using namespace Microsoft::WRL;

namespace mofu::graphics::d3d12::d3dx {
   
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
		DXCall(core::Device()->CreateCommittedResource2(isCpuAccessible ? &HeapProperties.UPLOAD_HEAP : &HeapProperties.DEFAULT_HEAP,
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

void 
TransitionResource(DXResource* resource, DXGraphicsCommandList* cmdList, 
	D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, 
	D3D12_RESOURCE_BARRIER_FLAGS flags, u32 subresource)
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = flags;
	barrier.Transition.pResource = resource;
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	barrier.Transition.Subresource = subresource;

	cmdList->ResourceBarrier(1, &barrier);
}

ID3D12RootSignature* 
CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC1& desc)
{
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedDesc{};
	versionedDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versionedDesc.Desc_1_1 = desc;

	HRESULT hr{ S_OK };
	ComPtr<ID3DBlob> errorBlob{ nullptr };
	ComPtr<ID3DBlob> rootSigBlob{ nullptr };
	if (FAILED(hr = D3D12SerializeVersionedRootSignature(&versionedDesc, &rootSigBlob, &errorBlob)))
	{
		DEBUG_OP(const char* errorMsg{ errorBlob ? (const char*)errorBlob->GetBufferPointer() : "" });
		DEBUG_LOG(errorMsg);
		return nullptr;
	}

	ID3D12RootSignature* rootSig{ nullptr };
	DXCall(hr = core::Device()->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), 
		rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSig)));
	if (FAILED(hr))
	{
		core::Release(rootSig);
	}

	return rootSig;
}

ID3D12PipelineState* 
CreatePipelineState(D3D12_PIPELINE_STATE_STREAM_DESC desc)
{
	assert(desc.SizeInBytes && desc.pPipelineStateSubobjectStream && desc.SizeInBytes >= sizeof(void*));

	ID3D12PipelineState* pso{ nullptr };
	DXCall(core::Device()->CreatePipelineState(&desc, IID_PPV_ARGS(&pso)));
	assert(pso);
	return pso;
}

ID3D12PipelineState* 
CreatePipelineState(void* stream, u64 streamSize)
{
	assert(stream && streamSize);
	D3D12_PIPELINE_STATE_STREAM_DESC desc{};
	desc.SizeInBytes = streamSize;
	desc.pPipelineStateSubobjectStream = stream;

	ID3D12PipelineState* pso{ nullptr };
	DXCall(core::Device()->CreatePipelineState(&desc, IID_PPV_ARGS(&pso)));
	assert(pso);
	return pso;
}

}
