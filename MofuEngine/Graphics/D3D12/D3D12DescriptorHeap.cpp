#include "D3D12DescriptorHeap.h"
#include <numeric>

namespace mofu::graphics::d3d12 {
bool 
DescriptorHeap::Initialize(u32 capacity, bool isShaderVisible)
{
	std::lock_guard lock{ _mutex };

	assert(capacity > 0 && capacity < D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2);
	assert(!(_type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER && capacity > D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE));
	if (_type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV || _type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV) isShaderVisible = false;
	Release();

	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Type = _type;
	desc.Flags = isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;
	desc.NumDescriptors = capacity;

	DXDevice* const device{ core::Device() };
	HRESULT hr{ S_OK };
	DXCall(hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_heap)));
	if (FAILED(hr)) return false;

	_freeHandles = std::make_unique<u32[]>(capacity);
	std::iota(_freeHandles.get(), _freeHandles.get() + capacity, 0u);

	_capacity = capacity;
	_descriptorCount = 0;
	DEBUG_OP(for (u32 i{ 0 }; i < FRAME_BUFFER_COUNT; ++i) assert(_deferredFreeIndices[i].empty()));

	_descriptorSize = device->GetDescriptorHandleIncrementSize(_type);
	_cpuStart = _heap->GetCPUDescriptorHandleForHeapStart();
	_gpuStart = isShaderVisible ? _heap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };

	return true;
}

void 
DescriptorHeap::Release()
{
	assert(_descriptorCount == 0);
	core::DeferredRelease(_heap);
}

void 
DescriptorHeap::ProcessDeferredFree(u32 frameId)
{
	std::lock_guard lock{ _mutex };
	assert(frameId < FRAME_BUFFER_COUNT);

	Vec<u32>& indices{ _deferredFreeIndices[frameId] };
	if (!indices.empty())
	{
		if (_capacity == 4096)
		{
			int a = 5;
			OutputDebugStringA("ASdasd");
			_descriptorCount += 1;
		}
		for (auto& id : indices)
		{
			--_descriptorCount;
			_freeHandles[_descriptorCount] = id;
		}
		indices.clear();
	}
}

DescriptorHandle 
DescriptorHeap::AllocateDescriptor()
{
	std::lock_guard lock{ _mutex };
	assert(_heap);
	assert(_descriptorCount < _capacity);

	if (_capacity == 4096)
	{
		OutputDebugStringA("Allocated SRV\n");
	}

	const u32 freeIndex{ _freeHandles[_descriptorCount] };
	const u32 offset{ freeIndex * _descriptorSize };

	++_descriptorCount;
	DescriptorHandle handle;
	handle.cpu.ptr = _cpuStart.ptr + offset;
	if(IsShaderVisible()) handle.gpu.ptr = _gpuStart.ptr + offset;
	handle.index = freeIndex;
	DEBUG_OP(handle.container = this);

	return handle;
}

void 
DescriptorHeap::FreeDescriptor(DescriptorHandle& handle)
{
	if (!handle.IsValid()) return;
	std::lock_guard lock{ _mutex };
	assert(_heap && _descriptorCount);
	assert(handle.container == this);
	assert(handle.cpu.ptr >= _cpuStart.ptr);
	assert(((handle.cpu.ptr - _cpuStart.ptr) % _descriptorSize) == 0);
	assert(handle.index < _capacity);

	assert(handle.index == (u32)((handle.cpu.ptr - _cpuStart.ptr) / _descriptorSize));

	_deferredFreeIndices[core::CurrentFrameIndex()].push_back(handle.index);
	core::SetHasDeferredReleases();
	handle = {};
}

}
