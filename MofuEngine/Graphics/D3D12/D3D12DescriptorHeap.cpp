#include "D3D12DescriptorHeap.h"
#include <numeric>

namespace mofu::graphics::d3d12 {
bool 
DescriptorHeap::Initialize(u32 persistentCapacity, u32 temporaryCapacity, bool isShaderVisible)
{
	std::lock_guard lock{ _mutex };

	assert(persistentCapacity > 0 && persistentCapacity < D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2);
	assert(!(_type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER && persistentCapacity > D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE));
	if (_type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV || _type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV) isShaderVisible = false;
	Release();

	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Type = _type;
	desc.Flags = isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;
	desc.NumDescriptors = persistentCapacity + temporaryCapacity;

	DXDevice* const device{ core::Device() };
	HRESULT hr{ S_OK };
	DXCall(hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_heap)));
	if (FAILED(hr)) return false;

	_freeHandles = std::make_unique<u32[]>(persistentCapacity);
	std::iota(_freeHandles.get(), _freeHandles.get() + persistentCapacity, 0u);

	_persistentCapacity = persistentCapacity;
	_persistentDescriptorCount = 0;
	DEBUG_OP(for (u32 i{ 0 }; i < FRAME_BUFFER_COUNT; ++i) assert(_deferredFreeIndices[i].empty()));
	_temporaryCapacity = temporaryCapacity;
	_temporaryDescriptorCount.store(0, std::memory_order_relaxed);

	_descriptorSize = device->GetDescriptorHandleIncrementSize(_type);
	_cpuStart = _heap->GetCPUDescriptorHandleForHeapStart();
	_gpuStart = isShaderVisible ? _heap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };

	return true;
}

void 
DescriptorHeap::Release()
{
	assert(core::CurrentFrameIndex() == 0);
	if (!_deferredFreeIndices[0].empty())
	{
		Vec<u32>& indices{ _deferredFreeIndices[0] };
		if (!indices.empty())
		{
			for (auto& id : indices)
			{
				--_persistentDescriptorCount;
				_freeHandles[_persistentDescriptorCount] = id;
			}
			indices.clear();
		}
	}
	assert(_persistentDescriptorCount == 0);
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
		for (auto& id : indices)
		{
			--_persistentDescriptorCount;
			_freeHandles[_persistentDescriptorCount] = id;
		}
		indices.clear();
	}
}

void
DescriptorHeap::EndFrame(u32 frameIdx)
{
	_temporaryDescriptorCount = 0;
}

DescriptorHandle 
DescriptorHeap::AllocateDescriptor()
{
	assert(_heap);
	assert(_persistentDescriptorCount < _persistentCapacity);

	std::lock_guard lock{ _mutex };
	const u32 freeIndex{ _freeHandles[_persistentDescriptorCount] };
	const u32 offset{ freeIndex * _descriptorSize };
	++_persistentDescriptorCount;

	DescriptorHandle handle;
	handle.cpu.ptr = _cpuStart.ptr + offset;
	if(IsShaderVisible()) handle.gpu.ptr = _gpuStart.ptr + offset;
	handle.index = freeIndex;
	DEBUG_OP(handle.container = this);

	return handle;
}

TempDescriptorAllocation 
DescriptorHeap::AllocateTemporary(u32 descriptorCount)
{
	assert(descriptorCount);
	const u32 tempIndex{ _temporaryDescriptorCount.fetch_add(descriptorCount, std::memory_order_relaxed) };
	assert(tempIndex < _temporaryCapacity);
	const u32 index{ tempIndex + _persistentCapacity };

	TempDescriptorAllocation temp{};
	temp.CPUStartHandle = _cpuStart;
	temp.CPUStartHandle.ptr += index * _descriptorSize;
	temp.GPUStartHandle = _gpuStart;
	temp.GPUStartHandle.ptr += index * _descriptorSize;
	temp.StartIndex = index;
	return temp;
}

void 
DescriptorHeap::FreeDescriptor(DescriptorHandle& handle)
{
	if (!handle.IsValid()) return;
	std::lock_guard lock{ _mutex };
	assert(_heap && _persistentDescriptorCount);
	assert(handle.container == this);
	assert(handle.cpu.ptr >= _cpuStart.ptr);
	assert(((handle.cpu.ptr - _cpuStart.ptr) % _descriptorSize) == 0);
	assert(handle.index < _persistentCapacity);

	assert(handle.index == (u32)((handle.cpu.ptr - _cpuStart.ptr) / _descriptorSize));

	_deferredFreeIndices[core::CurrentFrameIndex()].push_back(handle.index);
	core::SetHasDeferredReleases();
	handle = {};
}

}
