#include "D3D12Resources.h"

namespace mofu::graphics::d3d12 {

////////////////// D3D12 BUFFER //////////////////////////////////////////////////////
D3D12Buffer::D3D12Buffer(const D3D12BufferInitInfo& info, bool isCpuAccessible)
{
	assert(info.size && info.alignment);
	_size = (u32)math::AlignUp(info.size, info.alignment);
	_buffer = d3dx::CreateResourceBuffer(info.data, _size, isCpuAccessible, info.initialState, info.flags, info.heap, info.allocationInfo.Offset);
	_gpuAddress = _buffer->GetGPUVirtualAddress();
	NAME_D3D12_OBJECT_INDEXED(_buffer, _size, L"D3D12 Buffer - size:");
}

void 
D3D12Buffer::Release()
{
	core::DeferredRelease(_buffer);
	_size = 0;
	_gpuAddress = 0;
}

////////////////// CONSTANT BUFFER //////////////////////////////////////////////////////
ConstantBuffer::ConstantBuffer(const D3D12BufferInitInfo& info) : _buffer(info, true)
{
	NAME_D3D12_OBJECT_INDEXED(Buffer(), Size(), L"Constant Buffer - size:");
	D3D12_RANGE range{};
	DXCall(Buffer()->Map(0, &range, (void**)(&_cpuAddress)));
	assert(_cpuAddress);
}

u8* const 
ConstantBuffer::AllocateSpace(u32 size)
{
	std::lock_guard lock{ _mutex };
	const u32 alignedSize{ (u32)d3dx::AlignSizeForConstantBuffer(size) };
	assert(_cpuOffset + alignedSize <= _buffer.Size());
	if (_cpuOffset + alignedSize <= _buffer.Size())
	{
		u8* const address{ _cpuAddress + _cpuOffset };
		_cpuOffset += size;
		return address;
	}
	return nullptr;
}

}
