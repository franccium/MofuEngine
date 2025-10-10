#include "D3D12Resources.h"
#include "DirectXTex.h"

namespace mofu::graphics::d3d12 {

////////////////// BUFFER //////////////////////////////////////////////////////
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
		_cpuOffset += alignedSize;
		return address;
	}
	return nullptr;
}

////////////////// TEXTURE //////////////////////////////////////////////////////
D3D12Texture::D3D12Texture(D3D12TextureInitInfo info)
{
	DXDevice* const device{ core::Device() };

	D3D12_CLEAR_VALUE* const clearValue
	{
		(info.desc && 
			(info.desc->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET 
			|| info.desc->Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) ? &info.clearValue : nullptr
	};
	if (info.CreateUAV)
	{
		assert(info.desc);
		info.desc->Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	if (info.resource)
	{
		_resource = info.resource;
	}
	else if (info.heap)
	{
		assert(info.desc);
		DXCall(device->CreatePlacedResource1(info.heap, info.allocationInfo->Offset, 
			info.desc, info.initialState, clearValue, IID_PPV_ARGS(&_resource)));
	}
	else
	{
		assert(info.desc);
		DXCall(device->CreateCommittedResource2(&d3dx::HeapProperties.DEFAULT_HEAP, D3D12_HEAP_FLAG_NONE, 
			info.desc, info.initialState, clearValue, nullptr, IID_PPV_ARGS(&_resource)));
	}

	_srv = core::SrvHeap().AllocateDescriptor();
	device->CreateShaderResourceView(_resource, info.srvDesc, _srv.cpu);
}

void 
D3D12Texture::Release()
{
	core::SrvHeap().FreeDescriptor(_srv);
	core::DeferredRelease(_resource);
}

DescriptorHandle 
D3D12Texture::GetSRV(u32 arrayIndex, u32 mipLevel, u32 depthIndex, DXGI_FORMAT format, bool isCubemap)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	bool is3D = depthIndex > 0;
	if (isCubemap)
	{
		if (arrayIndex > 5)
		{
			u32 cubeIdx{ arrayIndex / 6 };
			u32 faceIdx{ arrayIndex % 6 };
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
			srvDesc.TextureCubeArray.MostDetailedMip = mipLevel;
			srvDesc.TextureCubeArray.MipLevels = 1;
			srvDesc.TextureCubeArray.First2DArrayFace = cubeIdx * 6 + faceIdx;
			srvDesc.TextureCubeArray.NumCubes = 1;
		}
		else
		{
			/*srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MostDetailedMip = mipLevel;
			srvDesc.TextureCube.MipLevels = 1;
			srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;*/
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MostDetailedMip = mipLevel;
			srvDesc.Texture2DArray.MipLevels = 1;
			srvDesc.Texture2DArray.FirstArraySlice = arrayIndex;
			srvDesc.Texture2DArray.ArraySize = 1;
			srvDesc.Texture2DArray.PlaneSlice = 0;
			srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
		}
	}
	else if (is3D)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MostDetailedMip = mipLevel;
		srvDesc.Texture3D.MipLevels = 1;
		srvDesc.Texture3D.ResourceMinLODClamp = 0.f;
	}
	else
	{
		if (arrayIndex > 0)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MostDetailedMip = mipLevel;
			srvDesc.Texture2DArray.MipLevels = 1;
			srvDesc.Texture2DArray.FirstArraySlice = arrayIndex;
			srvDesc.Texture2DArray.ArraySize = 1;
			srvDesc.Texture2DArray.PlaneSlice = 0;
			srvDesc.Texture2DArray.ResourceMinLODClamp = 0.f;
		}
		else
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = mipLevel;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.PlaneSlice = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.f;
		}
	}

	DescriptorHandle srv = core::SrvHeap().AllocateDescriptor();
	core::Device()->CreateShaderResourceView(_resource, &srvDesc, srv.cpu);

	return srv;
}

////////////////// RENDER TEXTURE ////////////////////////////////////////////////
D3D12RenderTexture::D3D12RenderTexture(D3D12TextureInitInfo info)
{
	assert(info.desc);

	//FIXME: temporary fix for embedding into a dockable ImGUI window
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = info.clearValue.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.f;
	assert(!info.srvDesc && !info.resource);
	info.srvDesc = &srvDesc;
	_texture = D3D12Texture{ info };

	_mipCount = Resource()->GetDesc1().MipLevels;
	assert(_mipCount && _mipCount <= D3D12Texture::MAX_MIPS);
	
	DescriptorHeap& heap{ core::RtvHeap() };
	D3D12_RENDER_TARGET_VIEW_DESC desc{};
	desc.Format = info.desc->Format;
	desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MipSlice = 0;

	DXDevice* const device{ core::Device() };
	for (u32 i{ 0 }; i < _mipCount; ++i)
	{
		_rtv[i] = heap.AllocateDescriptor();
		device->CreateRenderTargetView(Resource(), &desc, _rtv[i].cpu);
		++desc.Texture2D.MipSlice;
	}

	if (info.CreateUAV)
	{
		_uav = core::UavHeap().AllocateDescriptor();
		device->CreateUnorderedAccessView(Resource(), nullptr, nullptr, _uav.cpu);
	}
}

void 
D3D12RenderTexture::Release()
{
	for(u32 i{0}; i < _mipCount; ++i)
		core::RtvHeap().FreeDescriptor(_rtv[i]);
	core::UavHeap().FreeDescriptor(_uav);
	_texture.Release();
	_mipCount = 0;
}

////////////////// DEPTH BUFFER //////////////////////////////////////////
D3D12DepthBuffer::D3D12DepthBuffer(D3D12TextureInitInfo info)
{
	const DXGI_FORMAT dsvFormat{ info.desc->Format };
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	if (info.desc->Format == DXGI_FORMAT_D32_FLOAT)
	{
		info.desc->Format = DXGI_FORMAT_R32_TYPELESS;
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	}

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.f;

	assert(!info.srvDesc && !info.resource);
	info.srvDesc = &srvDesc;
	_texture = D3D12Texture(info);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = dsvFormat;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Texture2D.MipSlice = 0;

	DXDevice* const device{ core::Device() };
	_dsv = core::DsvHeap().AllocateDescriptor();
	device->CreateDepthStencilView(Resource(), &dsvDesc, _dsv.cpu);
}

void 
D3D12DepthBuffer::Release()
{
	core::DsvHeap().FreeDescriptor(_dsv);
	_texture.Release();
}

UAVClearableBuffer::UAVClearableBuffer(const D3D12BufferInitInfo& info)
	: _buffer{ info, false }
{
	assert(info.size && info.alignment);
	NAME_D3D12_OBJECT_INDEXED(Buffer(), info.size, L"UAV Clearable Buffer - size:");
	assert(info.flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	_uav = core::UavHeap().AllocateDescriptor();
	_uavShaderVisible = core::SrvHeap().AllocateDescriptor();

	D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
	desc.Format = DXGI_FORMAT_R32_UINT;
	desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	desc.Buffer.FirstElement = 0;
	desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	desc.Buffer.NumElements = _buffer.Size() / sizeof(u32); // for the R32_UINT format

	core::Device()->CreateUnorderedAccessView(Buffer(), nullptr, &desc, _uav.cpu);
	core::Device()->CopyDescriptorsSimple(1, _uavShaderVisible.cpu, _uav.cpu, core::SrvHeap().Type());
}

void 
UAVClearableBuffer::Release()
{
	core::SrvHeap().FreeDescriptor(_uavShaderVisible);
	core::UavHeap().FreeDescriptor(_uav);
	_buffer.Release();
}

StructuredBuffer::StructuredBuffer(const StructuredBufferInitInfo& info)
{
	Initialize(info);
}

void 
StructuredBuffer::Release()
{
	for (u32 i{ 0 }; i < FRAME_BUFFER_COUNT; ++i)
		core::SrvHeap().FreeDescriptor(_srv[i]);
	core::UavHeap().FreeDescriptor(_uav);
	core::UavHeap().FreeDescriptor(_counterUAV);
	core::Release(_counterResource);
	_buffer.Release();
	_stride = 0;
	_elementCount = 0;
}

void
StructuredBuffer::Initialize(const StructuredBufferInitInfo& info)
{
	Release();
	assert(info.Stride && info.ElementCount);
	DEBUG_OP(if (info.IsShaderTable) assert(info.Stride % D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT == 0));
	_stride = info.Stride;
	_elementCount = info.ElementCount;
	_isShaderTable = info.IsShaderTable;

	D3D12BufferInitInfo bufferInfo{};
	bufferInfo.heap = info.Heap;
	bufferInfo.data = info.InitialData;
	bufferInfo.allocationInfo = {};
	bufferInfo.initialState = info.InitialState;
	bufferInfo.flags = info.CreateUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
	bufferInfo.size = info.Stride * info.ElementCount;
	bufferInfo.alignment = info.Stride;
	_buffer = D3D12Buffer{ bufferInfo, info.IsCPUAccessible };

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{ SRVDesc(0) };
	for (u32 i{ 0 }; i < FRAME_BUFFER_COUNT; ++i)
	{
		_srv[i] = core::SrvHeap().AllocateDescriptor();
		core::Device()->CreateShaderResourceView(_buffer.Buffer(), &srvDesc, _srv[i].cpu);
	}

	if (info.CreateUAV)
	{
		assert(!info.IsDynamic);
		if (info.UseCounter)
		{
			D3D12_RESOURCE_DESC counterDesc{};
			counterDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			counterDesc.Alignment = 0;
			counterDesc.Width = sizeof(u32);
			counterDesc.Height = 1;
			counterDesc.DepthOrArraySize = 1;
			counterDesc.MipLevels = 1;
			counterDesc.Format = DXGI_FORMAT_UNKNOWN;
			counterDesc.SampleDesc.Count = 1;
			counterDesc.SampleDesc.Quality = 0;
			counterDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			counterDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			DXCall(core::Device()->CreateCommittedResource(&d3dx::HeapProperties.DEFAULT_HEAP, D3D12_HEAP_FLAG_NONE, &counterDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&_counterResource)));

			_counterUAV = core::UavHeap().AllocateDescriptor();
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
			uavDesc.Format = DXGI_FORMAT_UNKNOWN;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.NumElements = 1;
			uavDesc.Buffer.StructureByteStride = sizeof(u32);
			uavDesc.Buffer.CounterOffsetInBytes = 0;
			uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
			core::Device()->CreateUnorderedAccessView(_counterResource, nullptr, &uavDesc, _counterUAV.cpu);
		}

		_uav = core::UavHeap().AllocateDescriptor();
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = _elementCount;
		uavDesc.Buffer.StructureByteStride = _stride;
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		core::Device()->CreateUnorderedAccessView(_buffer.Buffer(), _counterResource, &uavDesc, _uav.cpu);
	}
	const wchar_t* const name{ info.Name ? info.Name : L"Structured Buffer: " };
	NAME_D3D12_OBJECT_INDEXED(_buffer.Buffer(), _stride * _elementCount, name);
}

void* 
StructuredBuffer::Map()
{
	return nullptr;
}

void
StructuredBuffer::MapAndSetData(const void* data, u32 elementCount)
{

}

D3D12_SHADER_RESOURCE_VIEW_DESC 
StructuredBuffer::SRVDesc(u32 bufferIdx) const
{
	assert(bufferIdx < FRAME_BUFFER_COUNT);
	assert(bufferIdx == 0 || _buffer.IsDynamic());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = _elementCount * bufferIdx;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc.Buffer.NumElements = _elementCount;
	srvDesc.Buffer.StructureByteStride = _stride;
	return srvDesc;
}

void 
StructuredBuffer::UpdateDynamicSRV() const
{
	/*assert(_buffer.IsDynamic());
	u32 currentBufferIdx{ core::CurrentFrameIndex() };
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{SRVDesc(currentBufferIdx)};
	core::Device()->CreateShaderResourceView(_buffer.Buffer(), &srvDesc, _srv[currentBufferIdx].cpu);
	core::CreateSRVDeferred(_buffer.Resource(), &srvDesc, );*/
}

D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE
StructuredBuffer::ShaderTable(u32 startElement, u32 elementCount) const
{
	assert(_isShaderTable && startElement < elementCount);

	elementCount = std::min(elementCount, _elementCount - startElement);
	D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE res{};
	res.StartAddress = GpuAddress() + _stride * startElement;
	res.SizeInBytes = elementCount * _stride;
	res.StrideInBytes = _stride;

	return res;
}

D3D12_GPU_VIRTUAL_ADDRESS_RANGE
StructuredBuffer::ShaderRecord(u32 elementIdx) const
{
	assert(_isShaderTable && elementIdx < _elementCount);

	D3D12_GPU_VIRTUAL_ADDRESS_RANGE res{};
	res.StartAddress = GpuAddress() + elementIdx * _stride;
	res.SizeInBytes = _stride;

	return res;
}

FormattedBuffer::FormattedBuffer(const FormattedBufferInitInfo& info)
{
	Initialize(info);
}

void
FormattedBuffer::Release()
{
	for(u32 i{0}; i < FRAME_BUFFER_COUNT; ++i)
		core::SrvHeap().FreeDescriptor(_srv[i]);
	core::UavHeap().FreeDescriptor(_uav);
	_buffer.Release();
	_stride = 0;
	_elementCount = 0;
}

void
FormattedBuffer::Initialize(const FormattedBufferInitInfo& info)
{
	Release();
	assert(info.Format != DXGI_FORMAT_UNKNOWN);
	assert(info.ElementCount);
	_format = info.Format;
	_stride = DirectX::BitsPerPixel(info.Format) / 8;
	_elementCount = info.ElementCount;

	D3D12BufferInitInfo bufferInfo{};
	bufferInfo.heap = info.Heap;
	bufferInfo.data = info.InitialData;
	bufferInfo.allocationInfo = {};
	bufferInfo.initialState = info.InitialState;
	bufferInfo.flags = info.CreateUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
	bufferInfo.size = _stride * info.ElementCount;
	bufferInfo.alignment = _stride;
	_buffer = D3D12Buffer{ bufferInfo, info.IsCPUAccessible };

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{ SRVDesc(0) };
	for (u32 i{ 0 }; i < FRAME_BUFFER_COUNT; ++i)
	{
		_srv[i] = core::SrvHeap().AllocateDescriptor();
		core::Device()->CreateShaderResourceView(_buffer.Buffer(), &srvDesc, _srv[i].cpu);
	}

	if (info.CreateUAV)
	{
		assert(!info.IsDynamic);

		_uav = core::UavHeap().AllocateDescriptor();
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = _format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = _elementCount;
		uavDesc.Buffer.StructureByteStride = _stride;
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		core::Device()->CreateUnorderedAccessView(_buffer.Buffer(), nullptr, &uavDesc, _uav.cpu);
	}
	const wchar_t* const name{ info.Name ? info.Name : L"Formatted Buffer: " };
	NAME_D3D12_OBJECT_INDEXED(_buffer.Buffer(), _stride * _elementCount, name);
}

void*
FormattedBuffer::Map()
{
	return nullptr;
}

void
FormattedBuffer::MapAndSetData(const void* data, u32 elementCount)
{

}

D3D12_INDEX_BUFFER_VIEW 
FormattedBuffer::IndexBufferView() const
{
	assert(_format == DXGI_FORMAT_R16_UINT || _format == DXGI_FORMAT_R32_UINT);
	D3D12_INDEX_BUFFER_VIEW ibv{};
	ibv.BufferLocation = GpuAddress();
	ibv.SizeInBytes = _buffer.Size();
	ibv.Format = _format;
	return ibv;
}

D3D12_SHADER_RESOURCE_VIEW_DESC
FormattedBuffer::SRVDesc(u32 bufferIdx) const
{
	assert(bufferIdx < FRAME_BUFFER_COUNT);
	assert(bufferIdx == 0 || _buffer.IsDynamic());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = _elementCount * bufferIdx;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc.Buffer.NumElements = _elementCount;
	srvDesc.Buffer.StructureByteStride = _stride;
	return srvDesc;
}

void
FormattedBuffer::UpdateDynamicSRV() const
{
	/*assert(_buffer.IsDynamic());
	u32 currentBufferIdx{ core::CurrentFrameIndex() };
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{ SRVDesc(currentBufferIdx) };
	core::Device()->CreateShaderResourceView(_buffer.Buffer(), &srvDesc, _srv[currentBufferIdx].cpu);
	core::CreateSRVDeferred(_buffer.Resource(), &srvDesc, );*/
}

RawBuffer::RawBuffer(const RawBufferInitInfo& info)
{
	Initialize(info);
}

void
RawBuffer::Release()
{
	for (u32 i{ 0 }; i < FRAME_BUFFER_COUNT; ++i)
		core::SrvHeap().FreeDescriptor(_srv[i]);
	core::UavHeap().FreeDescriptor(_uav);
	_buffer.Release();
	_elementCount = 0;
}

void
RawBuffer::Initialize(const RawBufferInitInfo& info)
{
	Release();
	assert(info.ElementCount);
	assert(!(info.CreateUAV && info.IsDynamic));
	assert(!info.IsCPUAccessible || info.IsDynamic);
	_elementCount = info.ElementCount;

	D3D12BufferInitInfo bufferInfo{};
	bufferInfo.heap = info.Heap;
	bufferInfo.data = info.InitialData;
	bufferInfo.allocationInfo = {};
	bufferInfo.initialState = info.InitialState;
	bufferInfo.flags = info.CreateUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
	bufferInfo.size = Stride * info.ElementCount;
	bufferInfo.alignment = Stride;
	_buffer = D3D12Buffer{ bufferInfo, info.IsCPUAccessible };

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{ SRVDesc(0) };
	for (u32 i{ 0 }; i < FRAME_BUFFER_COUNT; ++i)
	{
		_srv[i] = core::SrvHeap().AllocateDescriptor();
		core::Device()->CreateShaderResourceView(_buffer.Buffer(), &srvDesc, _srv[i].cpu);
	}

	if (info.CreateUAV)
	{
		assert(!info.IsDynamic);

		_uav = core::UavHeap().AllocateDescriptor();
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = _elementCount;
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
		core::Device()->CreateUnorderedAccessView(_buffer.Buffer(), nullptr, &uavDesc, _uav.cpu);
	}
	const wchar_t* const name{ info.Name ? info.Name : L"Raw Buffer: " };
	NAME_D3D12_OBJECT_INDEXED(_buffer.Buffer(), Stride * _elementCount, name);
}

void*
RawBuffer::Map()
{
	return nullptr;
}

void
RawBuffer::MapAndSetData(const void* data, u32 elementCount)
{

}

D3D12_SHADER_RESOURCE_VIEW_DESC
RawBuffer::SRVDesc(u32 bufferIdx) const
{
	assert(bufferIdx < FRAME_BUFFER_COUNT);
	assert(bufferIdx == 0 || _buffer.IsDynamic());
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = _elementCount * bufferIdx;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	srvDesc.Buffer.NumElements = _elementCount;
	srvDesc.Buffer.StructureByteStride = 0;
	return srvDesc;
}

}
