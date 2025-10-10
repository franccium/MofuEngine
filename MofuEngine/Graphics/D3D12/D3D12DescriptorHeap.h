#pragma once

#include "D3D12CommonHeaders.h"
#include "D3D12Core.h"
#include <atomic>

namespace mofu::graphics::d3d12 {
struct DescriptorHandle;
struct TempDescriptorAllocation;
class DescriptorHeap

{
public:
	explicit DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type) : _type{ type } {}
	DISABLE_COPY_AND_MOVE(DescriptorHeap);
	~DescriptorHeap() { assert(!_heap); }

	bool Initialize(u32 persistentCapacity, u32 temporaryCapacity, bool isShaderVisible);
	void Release();
	void ProcessDeferredFree(u32 frameId);
	void EndFrame(u32 frameId);

	[[nodiscard]] DescriptorHandle AllocateDescriptor();
	[[nodiscard]] TempDescriptorAllocation AllocateTemporary(u32 descriptorCount);
	void FreeDescriptor(DescriptorHandle& handle);

	[[nodiscard]] constexpr D3D12_DESCRIPTOR_HEAP_TYPE Type() const { return _type; }
	[[nodiscard]] constexpr ID3D12DescriptorHeap* const Heap() const { return _heap; }
	[[nodiscard]] constexpr D3D12_CPU_DESCRIPTOR_HANDLE CpuStart() const { return _cpuStart; }
	[[nodiscard]] constexpr D3D12_GPU_DESCRIPTOR_HANDLE GpuStart() const { return _gpuStart; }
	[[nodiscard]] constexpr u32 Capacity() const { return _persistentCapacity; }
	[[nodiscard]] constexpr u32 DescriptorCount() const { return _persistentDescriptorCount; }
	[[nodiscard]] constexpr u32 DescriptorSize() const { return _descriptorSize; }
	[[nodiscard]] constexpr bool IsShaderVisible() const { return _gpuStart.ptr != 0; }

private:
	const D3D12_DESCRIPTOR_HEAP_TYPE _type{};
	ID3D12DescriptorHeap* _heap{ nullptr };
	std::mutex _mutex{};

	D3D12_CPU_DESCRIPTOR_HANDLE _cpuStart; // base address for the heap accessible by the cpu
	D3D12_GPU_DESCRIPTOR_HANDLE _gpuStart;

	std::unique_ptr<u32[]> _freeHandles{}; // pointers to free spaces on the heap
	Vec<u32> _deferredFreeIndices[FRAME_BUFFER_COUNT]{};
	u32 _persistentCapacity{ 0 };
	u32 _temporaryCapacity{ 0 };
	u32 _persistentDescriptorCount{ 0 };
	std::atomic<u32> _temporaryDescriptorCount{ 0 };
	u32 _descriptorSize{};
};
}