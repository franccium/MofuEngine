#pragma once
#include "D3D12CommonHeaders.h"
#include "D3D12Core.h"
#include "D3D12DescriptorHeap.h"

// Provides utility classes for D3D12 resource management

namespace mofu::graphics::d3d12 {
struct DescriptorHandle
{
	constexpr DescriptorHandle() = default;
	constexpr DescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) { cpu = cpuHandle; gpu = gpuHandle; }
	D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
	D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
	u32 index{ U32_INVALID_ID }; //TODO: check

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

class UAVClearableBuffer
{
public:
	UAVClearableBuffer() = default;
	UAVClearableBuffer(const D3D12BufferInitInfo& info);
	DISABLE_COPY(UAVClearableBuffer);

	constexpr UAVClearableBuffer(UAVClearableBuffer&& o) 
		: _buffer{ std::move(o._buffer) }, _uav{ o._uav }, _uavShaderVisible{ o._uavShaderVisible } 
	{ o.Reset(); }

	constexpr UAVClearableBuffer& operator=(UAVClearableBuffer&& o)
	{
		assert(this != &o);
		if (this != &o)
		{
			Release();
			Move(o);
		}
		return *this;
	}

	~UAVClearableBuffer() { Release(); }

	void Release();

	void ClearUAV(DXGraphicsCommandList* const cmdList, const u32* const clearValues) const
	{
		assert(Buffer());
		assert(_uav.IsValid() && _uavShaderVisible.IsValid() && _uavShaderVisible.IsShaderVisible());
		cmdList->ClearUnorderedAccessViewUint(_uavShaderVisible.gpu, _uav.cpu, Buffer(), clearValues, 0, nullptr);
	}

	void ClearUAV(DXGraphicsCommandList* const cmdList, const f32* const clearValues) const
	{
		assert(Buffer());
		assert(_uav.IsValid() && _uavShaderVisible.IsValid() && _uavShaderVisible.IsShaderVisible());
		cmdList->ClearUnorderedAccessViewFloat(_uavShaderVisible.gpu, _uav.cpu, Buffer(), clearValues, 0, nullptr);
	}

	[[nodiscard]] constexpr DXResource* Buffer() const { return _buffer.Buffer(); }
	[[nodiscard]] constexpr D3D12_GPU_VIRTUAL_ADDRESS GpuAdress() const { return _buffer.GpuAddress(); }
	[[nodiscard]] constexpr u32 Size() const { return _buffer.Size(); }
	[[nodiscard]] constexpr DescriptorHandle UAV() const { return _uav; }
	[[nodiscard]] constexpr DescriptorHandle UAVShaderVisible() const { return _uavShaderVisible; }

	[[nodiscard]] constexpr static D3D12BufferInitInfo GetDefaultInitInfo(u32 size)
	{
		assert(size);
		D3D12BufferInitInfo info{};
		info.size = size;
		info.alignment = sizeof(v4);
		info.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		return info;
	}

private:
	constexpr void Move(UAVClearableBuffer& o)
	{
		_buffer = std::move(o._buffer);
		_uav = o._uav;
		_uavShaderVisible = o._uavShaderVisible;
		o.Reset();
	}

	constexpr void Reset()
	{
		_uav = {};
		_uavShaderVisible = {};
	}

	D3D12Buffer _buffer{};
	DescriptorHandle _uav;
	DescriptorHandle _uavShaderVisible;
};

struct D3D12TextureInitInfo
{
	DXResource* resource{ nullptr };
	D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc{ nullptr };
	D3D12_RESOURCE_DESC1* desc{ nullptr };
	ID3D12Heap1* heap{ nullptr };
	D3D12_RESOURCE_ALLOCATION_INFO1* allocationInfo{ nullptr };
	D3D12_RESOURCE_STATES initialState{};
	D3D12_CLEAR_VALUE clearValue{};
};

class D3D12Texture
{
public:
	constexpr static u32 MAX_MIPS{ 14 };

	D3D12Texture() = default;
	explicit D3D12Texture(D3D12TextureInitInfo info);
	DISABLE_COPY(D3D12Texture);
	constexpr D3D12Texture(D3D12Texture&& o) : _resource{ o._resource }, _srv{ o._srv } { o.Reset(); }
	constexpr D3D12Texture& operator=(D3D12Texture&& o)
	{
		assert(this != &o);
		if (this != &o)
		{
			Release();
			Move(o);
		}
		return *this;
	}
	~D3D12Texture() { Release(); }

	void Release();

	[[nodiscard]] DescriptorHandle GetSRV(u32 arrayindex, u32 mipLevel, u32 depthIndex,
		DXGI_FORMAT format, bool isCubemap = false);

	[[nodiscard]] constexpr DXResource* const Resource() const { return _resource; }
	[[nodiscard]] constexpr DescriptorHandle Srv() const { return _srv; }

private:
	constexpr void Reset()
	{
		_resource = nullptr;
		_srv = {};
	}

	constexpr void Move(D3D12Texture& o)
	{
		_resource = o._resource;
		_srv = o._srv;
		o.Reset();
	}

	DXResource* _resource{ nullptr };
	DescriptorHandle _srv;
};

class D3D12RenderTexture
{
public:
	D3D12RenderTexture() = default;
	explicit D3D12RenderTexture(D3D12TextureInitInfo info);
	DISABLE_COPY(D3D12RenderTexture);

	constexpr D3D12RenderTexture(D3D12RenderTexture&& o) : _texture{ std::move(o._texture) }, _mipCount{ o._mipCount }
	{
		for (u32 i{ 0 }; i < _mipCount; ++i) _rtv[i] = o._rtv[i];
		o.Reset();
	}

	constexpr D3D12RenderTexture& operator=(D3D12RenderTexture&& o)
	{
		assert(this != &o);
		if (this != &o)
		{
			Release();
			Move(o);
		}
		return *this;
	}

	~D3D12RenderTexture() { Release(); }

	void Release();

	[[nodiscard]] constexpr u32 MipCount() const { return _mipCount; }
	[[nodiscard]] constexpr D3D12_CPU_DESCRIPTOR_HANDLE Rtv(u32 mipIndex) const { assert(mipIndex < _mipCount);  return _rtv[mipIndex].cpu; }
	[[nodiscard]] constexpr DescriptorHandle Srv() const { return _texture.Srv(); }
	[[nodiscard]] constexpr DXResource* const Resource() const { return _texture.Resource(); }

private:
	constexpr void Reset()
	{
		for (u32 i{ 0 }; i < _mipCount; ++i) _rtv[i] = {};
		_mipCount = 0;
	}

	constexpr void Move(D3D12RenderTexture& o)
	{
		_texture = std::move(o._texture);
		_mipCount = o._mipCount;
		for (u32 i{ 0 }; i < _mipCount; ++i) _rtv[i] = o._rtv[i];
		o.Reset();
	}

	D3D12Texture _texture;
	DescriptorHandle _rtv[D3D12Texture::MAX_MIPS]{};
	u32 _mipCount{ 0 };
};

class D3D12DepthBuffer
{
public:
	D3D12DepthBuffer() = default;
	explicit D3D12DepthBuffer(D3D12TextureInitInfo info);
	DISABLE_COPY(D3D12DepthBuffer);

	constexpr D3D12DepthBuffer(D3D12DepthBuffer&& o) : _texture{ std::move(o._texture) }, _dsv{ o._dsv }
	{
		o._dsv = {};
	}

	constexpr D3D12DepthBuffer& operator=(D3D12DepthBuffer&& o)
	{
		assert(this != &o);
		if (this != &o)
		{
			_texture = std::move(o._texture);
			_dsv = o._dsv;
			o._dsv = {};
		}
		return *this;
	}

	~D3D12DepthBuffer() { Release(); }

	void Release();

	[[nodiscard]] constexpr D3D12_CPU_DESCRIPTOR_HANDLE Dsv() const { return _dsv.cpu; }
	[[nodiscard]] constexpr DescriptorHandle Srv() const { return _texture.Srv(); }
	[[nodiscard]] constexpr DXResource* const Resource() const { return _texture.Resource(); }

private:
	D3D12Texture _texture;
	DescriptorHandle _dsv{};
};
}