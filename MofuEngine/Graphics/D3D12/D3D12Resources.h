#pragma once
#include "D3D12CommonHeaders.h"
#include "D3D12Core.h"
#include "D3D12DescriptorHeap.h"

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

struct D3D12BufferInitInfo
{
	DXHeap* heap{ nullptr };
	const void* data{ nullptr };
	D3D12_RESOURCE_ALLOCATION_INFO1 allocationInfo{};
	D3D12_RESOURCE_STATES initialState{ D3D12_RESOURCE_STATE_COMMON };
	D3D12_RESOURCE_FLAGS flags{ D3D12_RESOURCE_FLAG_NONE };
	u32 size{ 0 };
	u32 alignment{ 0 };
};

// Contains a buffer mapeed to the GPU memory
class D3D12Buffer
{
public:
	D3D12Buffer() = default;
	explicit D3D12Buffer(const D3D12BufferInitInfo& info, bool isCpuAccessible);
	DISABLE_COPY(D3D12Buffer);
	constexpr D3D12Buffer(D3D12Buffer&& o) : _buffer{ o._buffer }, _gpuAddress{ o._gpuAddress }, _size{ o._size }
	{
		o.Reset();
	}

	constexpr D3D12Buffer& operator=(D3D12Buffer&& o)
	{
		assert(this != &o);
		if (this != &o)
		{
			Release();
			Move(o);
		}
		return *this;
	}
	~D3D12Buffer() { Release(); }

	void Release();

	[[nodiscard]] constexpr DXResource* const Buffer() const { return _buffer; }
	[[nodiscard]] constexpr D3D12_GPU_VIRTUAL_ADDRESS GpuAddress() const { return _gpuAddress; }
	[[nodiscard]] constexpr u32 Size() const { return _size; }

private:
	constexpr void Reset()
	{
		_buffer = nullptr;
		_gpuAddress = 0;
		_size = 0;
	}

	constexpr void Move(D3D12Buffer& o)
	{
		_buffer = o._buffer;
		_gpuAddress = o._gpuAddress;
		_size = o._size;
		o.Reset();
	}

	DXResource* _buffer{ nullptr };
	D3D12_GPU_VIRTUAL_ADDRESS _gpuAddress{ 0 };
	u32 _size{ 0 };
};

// A class for managing a cpu-accessible buffer
// acts as a linear stack allocator, with the stack being cleared after each frame
// can be accessed by different threads
class ConstantBuffer
{
public:
	ConstantBuffer() = default;
	explicit ConstantBuffer(const D3D12BufferInitInfo& info);
	DISABLE_COPY_AND_MOVE(ConstantBuffer);
	~ConstantBuffer() { Release(); }

	void Release()
	{
		_buffer.Release();
		_cpuAddress = nullptr;
		_cpuOffset = 0;
	}

	constexpr void Clear() { _cpuOffset = 0; }
	[[nodiscard]] u8* const AllocateSpace(u32 size);

	template<typename T>
	[[nodiscard]] constexpr T* const AllocateSpace()
	{
		return (T* const)AllocateSpace(sizeof(T));
	}

	[[nodiscard]] constexpr DXResource* const Buffer() const { return _buffer.Buffer(); }
	[[nodiscard]] constexpr D3D12_GPU_VIRTUAL_ADDRESS const GpuAddress() const { return _buffer.GpuAddress(); }
	[[nodiscard]] constexpr u32 Size() const { return _buffer.Size(); }
	[[nodiscard]] constexpr u8* CpuAddress() const { return _cpuAddress; }

	template<typename T>
	[[nodiscard]] constexpr D3D12_GPU_VIRTUAL_ADDRESS GpuAddress(T* const allocation)
	{
		assert(_cpuAddress);
		std::lock_guard lock{ _mutex };

		const u8* const address{ (const u8* const)allocation };
		assert(address >= _cpuAddress && address <= _cpuAddress + _cpuOffset);
		const u64 offset{ (u64)(address - _cpuAddress) };
		return _buffer.GpuAddress() + offset;
	}

	[[nodiscard]] constexpr static D3D12BufferInitInfo DefaultInitInfo(u32 size)
	{
		assert(size);
		D3D12BufferInitInfo info{};
		info.size = size;
		info.alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
		return info;
	}

private:
	D3D12Buffer _buffer;
	u8* _cpuAddress{ nullptr };
	u32 _cpuOffset{ 0 };
	std::mutex _mutex{};
};

}