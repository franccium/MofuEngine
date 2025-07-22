#include "D3D12Resources.h"

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
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
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
}

void 
D3D12RenderTexture::Release()
{
	for(u32 i{0}; i < _mipCount; ++i)
		core::RtvHeap().FreeDescriptor(_rtv[i]);
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

}
