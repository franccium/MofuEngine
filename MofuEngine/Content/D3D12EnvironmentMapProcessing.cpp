#include "D3D12EnvironmentMapProcessing.h"
#include "Content/Shaders/ContentProcessingShaders.h"
#include "Content/EngineShaders.h"
#include "Graphics/D3D12/D3D12CommonHeaders.h"
#include "Graphics/D3D12/D3D12Core.h"
#include "Graphics/D3D12/D3D12Upload.h"
#include "Graphics/GraphicsTypes.h"

using namespace DirectX;
using namespace mofu::graphics::d3d12;

namespace mofu::content::texture {
namespace {
constexpr u32 PREFILTERED_DIFFUSE_CUBEMAP_SIZE{ 64 };
constexpr u32 PREFILTERED_SPECULAR_CUBEMAP_SIZE{ 512 };
constexpr u32 ROUGHNESS_MIP_LEVELS{ 6 };
constexpr u32 BRDF_INTEGRATION_LUT_SIZE{ 512 };

struct CubemapProcessingRootParameter
{
	enum Parameter : u32
	{
		Constants,
		Output,

		Count
	};
};

struct ShaderConstants
{
	u32 CubemapsSrvIndex;
	u32 EnvMapSrvIndex;

	u32 CubeMapInSize;
	u32 CubeMapOutSize;
	u32 SampleCount; // used for prefiltered, but also as a sign to mirror the cubemap when its == 1
	f32 Roughness;
	u32 OutOffset;
};
static_assert(sizeof(ShaderConstants) == sizeof(hlsl::content::EnvironmentProcessingConstants));

struct EnvironmentProcessingParameters
{
	UAVClearableBuffer Output{};
	D3D12Buffer Output2{};
	ShaderConstants Constants{};
	DescriptorHandle OutputUAV{};
};
EnvironmentProcessingParameters parameters{};

ID3D12RootSignature* environmentProcessingRootSignature{ nullptr };
ID3D12PipelineState* equirectangularToCubemapPSO{ nullptr };
ID3D12PipelineState* diffusePrefilterPSO{ nullptr };
ID3D12PipelineState* specularPrefilterPSO{ nullptr };
ID3D12PipelineState* computeBrdfLutPSO{ nullptr };

bool
CreateEnvironmentProcessingRootSignature()
{
	assert(!environmentProcessingRootSignature);
	using param = CubemapProcessingRootParameter::Parameter;
	d3dx::D3D12RootParameter parameters[param::Count]{};
	parameters[param::Constants].AsCBV(D3D12_SHADER_VISIBILITY_ALL, 0);
	parameters[param::Output].AsUAV(D3D12_SHADER_VISIBILITY_ALL, 0);

	const D3D12_STATIC_SAMPLER_DESC samplers[]
	{
		d3dx::StaticSampler(d3dx::SamplerState.STATIC_LINEAR, 0, 0, D3D12_SHADER_VISIBILITY_ALL),
	};

	environmentProcessingRootSignature = d3dx::D3D12RootSignatureDesc
	{
		&parameters[0], param::Count, D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED,
		&samplers[0], _countof(samplers)
	}.Create();

	NAME_D3D12_OBJECT(environmentProcessingRootSignature, L"Cubemap Processing Root Signature");

	return environmentProcessingRootSignature != nullptr;
}

bool
CreatePSOs()
{
	{
		assert(!equirectangularToCubemapPSO);

		struct
		{
			d3dx::D3D12PipelineStateSubobjectRootSignature rootSig{ environmentProcessingRootSignature };
			d3dx::D3D12PipelineStateSubobjectCS cs{ shaders::content::GetContentShader(shaders::content::ContentShader::EnvMapProcessing_EquirectangularToCubeMapCS) };
		} stream;

		equirectangularToCubemapPSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
		NAME_D3D12_OBJECT(equirectangularToCubemapPSO, L"Equirectangular To Cubemap PSO");
	}

	{
		assert(!diffusePrefilterPSO);

		struct
		{
			d3dx::D3D12PipelineStateSubobjectRootSignature rootSig{ environmentProcessingRootSignature };
			d3dx::D3D12PipelineStateSubobjectCS cs{ shaders::content::GetContentShader(shaders::content::ContentShader::EnvMapProcessing_PrefilterDiffuseEnvMapCS) };
		} stream;

		diffusePrefilterPSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
		NAME_D3D12_OBJECT(diffusePrefilterPSO, L"Diffuse Prefilter PSO");
	}

	{
		assert(!specularPrefilterPSO);

		struct
		{
			d3dx::D3D12PipelineStateSubobjectRootSignature rootSig{ environmentProcessingRootSignature };
			d3dx::D3D12PipelineStateSubobjectCS cs{ shaders::content::GetContentShader(shaders::content::ContentShader::EnvMapProcessing_PrefilterSpecularEnvMapCS) };
		} stream;

		specularPrefilterPSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
		NAME_D3D12_OBJECT(specularPrefilterPSO, L"Specular Prefilter PSO");
	}

	{
		assert(!computeBrdfLutPSO);

		struct
		{
			d3dx::D3D12PipelineStateSubobjectRootSignature rootSig{ environmentProcessingRootSignature };
			d3dx::D3D12PipelineStateSubobjectCS cs{ shaders::content::GetContentShader(shaders::content::ContentShader::EnvMapProcessing_ComputeBRDFIntegrationLUTCS) };
		} stream;

		computeBrdfLutPSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
		NAME_D3D12_OBJECT(computeBrdfLutPSO, L"Compute BRDF LUT PSO");
	}

	D3D12BufferInitInfo info = UAVClearableBuffer::GetDefaultInitInfo(1);
	parameters.Output = UAVClearableBuffer{ info };
	NAME_D3D12_OBJECT(parameters.Output.Buffer(), L"Environment Processing Output Buffer");

	return equirectangularToCubemapPSO && diffusePrefilterPSO && specularPrefilterPSO && computeBrdfLutPSO && parameters.Output.Buffer();
}

DescriptorHandle
CopyImagesToSRV(const ScratchImage& imagesScratch, u32 width, u32 height, u32 mipLevels,
	u32 arraySize, DXGI_FORMAT format, DXDevice* const device, DXGraphicsCommandList* const cmdList)
{
	D3D12_RESOURCE_DESC1 textureDesc{};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Alignment = 0;
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.DepthOrArraySize = (u16)arraySize;
	textureDesc.MipLevels = mipLevels;
	textureDesc.Format = format;
	textureDesc.SampleDesc = { 1,0 };
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	const u32 imageCount{ (u32)imagesScratch.GetImageCount() };
	const Image* images{ imagesScratch.GetImages() };

	u64 totalRequiredSize{ 0 };
	const u32 subresourceCount{ mipLevels * arraySize };
	Array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(subresourceCount);
	Array<u32> numRows(subresourceCount);
	Array<u64> rowSizes(subresourceCount);

	for (u32 i{ 0 }; i < subresourceCount; ++i)
	{
		u64 requiredSize{ 0 };
		device->GetCopyableFootprints1(&textureDesc, i, 1, totalRequiredSize, &layouts[i], &numRows[i], &rowSizes[i], &requiredSize);
		totalRequiredSize += requiredSize;
	}

	assert(totalRequiredSize && totalRequiredSize < UINT_MAX);
	upload::D3D12UploadContext uploadContext{ (u32)totalRequiredSize };

	for (u32 i{ 0 }; i < subresourceCount; ++i)
	{
		const Image& src{ images[i] };

		u8* const dstData{ ((u8*)uploadContext.CpuAddress()) + layouts[i].Offset };
		const u8* const srcData{ static_cast<const u8*>(src.pixels) };

		for (u32 row{ 0 }; row < numRows[i]; ++row)
		{
			memcpy(dstData + row * layouts[i].Footprint.RowPitch, srcData + row * src.rowPitch, rowSizes[i]);
		}
	}

	DXResource* textureResource{ nullptr };
	device->CreateCommittedResource2(&d3dx::HeapProperties.DEFAULT_HEAP, D3D12_HEAP_FLAG_NONE, &textureDesc,
		D3D12_RESOURCE_STATE_COMMON, nullptr, nullptr, IID_PPV_ARGS(&textureResource));

	DXResource* const uploadBuffer{ uploadContext.UploadBuffer() };
	for (u32 i{ 0 }; i < subresourceCount; ++i)
	{
		D3D12_TEXTURE_COPY_LOCATION copySrc{};
		copySrc.pResource = uploadBuffer;
		copySrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		copySrc.PlacedFootprint = layouts[i];

		D3D12_TEXTURE_COPY_LOCATION dest{};
		dest.pResource = textureResource;
		dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dest.SubresourceIndex = i;

		uploadContext.CommandList()->CopyTextureRegion(&dest, 0, 0, 0, &copySrc, nullptr);
	}

	uploadContext.EndUpload();

	d3dx::TransitionResource(textureResource, cmdList, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	assert(textureResource);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = mipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	//TempDescriptorAllocation textureAlloc{ core::SrvHeap().AllocateTemporary(1) };
	DescriptorHandle textureHandle = core::SrvHeap().AllocateDescriptor();
	device->CreateShaderResourceView(textureResource, &srvDesc, textureHandle.cpu);
	return textureHandle;
}

void
DispatchEquirectangularToCubemap(DXGraphicsCommandList* const cmdList, const ShaderConstants& constants, u32 size)
{
	assert(constants.EnvMapSrvIndex != 0);
	ConstantBuffer& cbuffer{ core::CBuffer() };
	hlsl::content::EnvironmentProcessingConstants* const buffer{ cbuffer.AllocateSpace<hlsl::content::EnvironmentProcessingConstants>() };
	memcpy(buffer, &constants, sizeof(hlsl::content::EnvironmentProcessingConstants));

	d3dx::TransitionResource(parameters.Output2.Buffer(), cmdList, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cmdList->SetComputeRootSignature(environmentProcessingRootSignature);
	cmdList->SetPipelineState(equirectangularToCubemapPSO);

	cmdList->SetComputeRootConstantBufferView(CubemapProcessingRootParameter::Constants, cbuffer.GpuAddress(buffer));
	cmdList->SetComputeRootUnorderedAccessView(CubemapProcessingRootParameter::Output, parameters.Output2.GpuAddress());

	const u32 groupCount{ (size + 15) >> 4 }; // threads(16, 16, 1)
	cmdList->Dispatch(groupCount, groupCount, 6);
}

void
DispatchPrefilter(DXGraphicsCommandList* const cmdList, const hlsl::content::EnvironmentProcessingConstants& constants, u32 size, bool diffuse)
{
	ConstantBuffer& cbuffer{ core::CBuffer() };
	hlsl::content::EnvironmentProcessingConstants* const buffer{ cbuffer.AllocateSpace<hlsl::content::EnvironmentProcessingConstants>() };
	memcpy(buffer, &constants, sizeof(hlsl::content::EnvironmentProcessingConstants));

	const u32v4 clearValue{ 0,0,0,0 };
	parameters.Output.ClearUAV(cmdList, &clearValue.x);

	cmdList->SetComputeRootSignature(environmentProcessingRootSignature);
	cmdList->SetPipelineState(diffuse ? diffusePrefilterPSO : specularPrefilterPSO);

	cmdList->SetComputeRootConstantBufferView(CubemapProcessingRootParameter::Constants, cbuffer.GpuAddress(buffer));
	cmdList->SetComputeRootUnorderedAccessView(CubemapProcessingRootParameter::Output, parameters.Output2.GpuAddress());

	const u32 groupCount{ (size + 15) >> 4 }; // threads(16, 16, 1)
	cmdList->Dispatch(groupCount, groupCount, 6);
}

}

bool
InitializeEnvironmentProcessing()
{
	return CreateEnvironmentProcessingRootSignature() && CreatePSOs();
}

void
ShutdownEnvironmentProcessing()
{
	assert(environmentProcessingRootSignature && equirectangularToCubemapPSO && diffusePrefilterPSO && specularPrefilterPSO && computeBrdfLutPSO);
	core::DeferredRelease(environmentProcessingRootSignature);
	core::DeferredRelease(equirectangularToCubemapPSO);
	core::DeferredRelease(diffusePrefilterPSO);
	core::DeferredRelease(specularPrefilterPSO);
	core::DeferredRelease(computeBrdfLutPSO);
}

HRESULT
DownloadTexture2DArray(ScratchImage& outScratch, u32 width, u32 height, u32 arraySize, u32 mipLevels, DXGI_FORMAT format, bool isCubemap, DXResource* gpuResource, DXGraphicsCommandList* const cmdList)
{
	DXDevice* const device{ core::Device() };

	HRESULT hr{isCubemap ? outScratch.InitializeCube(format, width, height, arraySize / 6, mipLevels)
		: outScratch.Initialize2D(format, width, height, arraySize, mipLevels) };
	if (FAILED(hr)) return hr;

	D3D12_RESOURCE_DESC1 readDesc{ gpuResource->GetDesc1() };
	readDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	readDesc.Width = gpuResource->GetDesc1().Width;
	readDesc.Height = 1;
	readDesc.DepthOrArraySize = 1;
	readDesc.MipLevels = 1;
	readDesc.Format = DXGI_FORMAT_UNKNOWN;
	readDesc.SampleDesc.Count = 1;
	readDesc.SampleDesc.Quality = 0;
	readDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	readDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	readDesc.SamplerFeedbackMipRegion = {};
	
	DXResource* readbackBuffer;
	DXCall(device->CreateCommittedResource2(&d3dx::HeapProperties.READBACK_HEAP, D3D12_HEAP_FLAG_NONE, &readDesc, 
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, nullptr, IID_PPV_ARGS(&readbackBuffer)));
	cmdList->CopyResource(readbackBuffer, gpuResource);
	core::ExecuteCompute();
	
	D3D12_RESOURCE_DESC1 bufferDesc{ gpuResource->GetDesc1() };
	const u32 bufferSize{ (u32)bufferDesc.Width };
	D3D12_RANGE readRange{ 0, bufferSize };
	u8* mappedData{ nullptr };
	DXCall(readbackBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));

	const u32 bytesPerPixel{ sizeof(v4) }; // NOTE: assuming format
	
	assert(isCubemap);
	assert(height == width);
	assert(arraySize == 6);
	u32 mipWidth{ width };
	u32 mipOffset{ 0 };
	// layout for cubemaps is: buffer: mip0: face0-5, mip1: face0-5, ...
	for (u32 mip{ 0 }; mip < mipLevels; ++mip)
	{
		const u32 rowSize{ mipWidth * bytesPerPixel };
		const u32 faceSize{ mipWidth * rowSize };
		for (u32 face{ 0 }; face < arraySize; ++face)
		{
			const Image* img{ outScratch.GetImage(mip, face, 0) };

			u8* const src{ mappedData + mipOffset + face * faceSize };
			u8* const dst{ img->pixels };

			assert(img->slicePitch == mipWidth * img->rowPitch);
			memcpy(dst, src, rowSize * mipWidth);
		}
		mipWidth = std::max(mipWidth >> 1, 1u);
		mipOffset += faceSize * arraySize;
	}


	//u32 faceOffset{ 0 };
	// for cubemaps arrayIdx is face
	//for (u32 arrayIdx{ 0 }; arrayIdx < arraySize; ++arrayIdx) 
	//{
	//	//u32 faceSize{ width * height };
	//	//u32 faceRowPitch{ width * bytesPerPixel };
	//	u32 mipHeight{ height }

	//	for (u32 mip{ 0 }; mip < mipLevels; ++mip)
	//	{
	//		//faceSize = std::max(1u, faceSize >> mip);
	//		//faceRowPitch = std::max(1u, faceRowPitch >> mip);

	//		const Image* img{ outScratch.GetImage(mip, arrayIdx, 0) };

	//		u8* src{ mappedData + offsets[mip] };
	//		u8* dst{ img->pixels };

	//		/*for (u32 row{ 0 }; row < mipHeight; ++row) 
	//		{
	//			memcpy(dst, src, img->rowPitch);
	//			src += faceRowPitch;
	//			dst += img->rowPitch;
	//		}*/
	//		assert(img->slicePitch == mipHeight * img->rowPitch);
	//		for (u32 row{ 0 }; row < mipWidth; ++row)
	//		{
	//			memcpy(dst, src, rowSize);
	//			dst += img->rowPitch;
	//			src += rowSize;
	//		}
	//
	//		//faceOffset += faceSize * bytesPerPixel;
	//		mipHeight = std::max(1u, mipHeight >> 1);
	//	}
	//	//faceOffset += mipHeight * mipHeight * sizeof(v4) * mipLevels;
	//	faceOffset += totalSize;
	//}
	

	parameters.Output2.Release();
	//TODO: delete the resource
	readbackBuffer->Unmap(0, nullptr);
	core::DeferredRelease(readbackBuffer);

	return hr;
}

HRESULT 
EquirectangularToCubemapD3D12(const Image* envMaps, u32 envMapCount, u32 cubemapSize,
	bool usePrefilteredSize, bool isSpecular, bool mirrorCubemap, ScratchImage& outCubemaps)
{
	assert(envMapCount == 1); // creating one output buffer for now
	assert(envMaps && envMapCount);

	const DXGI_FORMAT format{ envMaps[0].format };
	const u32 bytesPerPixel{ sizeof(v4) }; // NOTE: assuming format is DXGI_FORMAT_R32G32B32A32_FLOAT
	assert(format == DXGI_FORMAT_R32G32B32A32_FLOAT);
	const u32 arraySize{ envMapCount * 6 };
	if (usePrefilteredSize) cubemapSize = PREFILTERED_SPECULAR_CUBEMAP_SIZE;

	ShaderConstants constants{};
	constants.CubeMapOutSize = cubemapSize;
	constants.SampleCount = mirrorCubemap ? 1 : 0;

	const u32 bufferSize{ cubemapSize * cubemapSize * arraySize * bytesPerPixel }; //NOTE: assuming format is DXGI_FORMAT_R32G32B32A32_FLOAT
	D3D12BufferInitInfo initInfo{};
	initInfo.heap = nullptr;
	initInfo.alignment = bytesPerPixel;
	initInfo.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	initInfo.size = bufferSize;
	parameters.Output2 = D3D12Buffer{ initInfo, false };

	DXGraphicsCommandList* const computeCommandList{ core::ComputeCommandList() };

	DXDevice* const device{ core::Device() };
	Vec<DXResource*> textureResources(envMapCount);
	Vec<DescriptorHandle> textureHandles(envMapCount);
	core::StartCompute();

	for (u32 i{ 0 }; i < envMapCount; ++i)
	{
		const Image& src{ envMaps[i] };

		D3D12_RESOURCE_DESC1 textureDesc{};
		assert(src.format == format);
		textureDesc.Format = format;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = (u32)src.width;
		textureDesc.Height = (u32)src.height;
		textureDesc.MipLevels = 1;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc = { 1, 0 };

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
		u32 numRows{};
		u64 rowSize{};
		u64 requiredSize{ 0 };
		device->GetCopyableFootprints1(&textureDesc, 0, 1, 0, &layout, &numRows, &rowSize, &requiredSize);
		assert(requiredSize && requiredSize < UINT_MAX);
		upload::D3D12UploadContext uploadContext{ (u32)requiredSize };
		u8* const cpuAddress{ (u8* const)uploadContext.CpuAddress() };

		D3D12_SUBRESOURCE_DATA textureData{};
		textureData.pData = src.pixels;
		textureData.RowPitch = src.rowPitch;
		textureData.SlicePitch = src.slicePitch;

		const u8* srcData{ static_cast<const u8*>(textureData.pData) };
		u8* dstData{ cpuAddress + layout.Offset };

		for (u32 row{ 0 }; row < numRows; ++row)
		{
			memcpy(dstData + row * layout.Footprint.RowPitch, srcData + row * textureData.RowPitch, rowSize);
		}

		device->CreateCommittedResource2(&d3dx::HeapProperties.DEFAULT_HEAP, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, nullptr, IID_PPV_ARGS(&textureResources[i]));

		DXResource* uploadBuffer{ uploadContext.UploadBuffer() };
		D3D12_TEXTURE_COPY_LOCATION copySrc{};
		copySrc.pResource = uploadBuffer;
		copySrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		copySrc.PlacedFootprint = layout;

		D3D12_TEXTURE_COPY_LOCATION dest{};
		dest.pResource = textureResources[i];
		dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dest.SubresourceIndex = 0;

		uploadContext.CommandList()->CopyTextureRegion(&dest, 0, 0, 0, &copySrc, nullptr);
		uploadContext.EndUpload();
		d3dx::TransitionResource(textureResources[i], computeCommandList, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		assert(textureResources[i]);
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;

		textureHandles[i] = core::SrvHeap().AllocateDescriptor();
		device->CreateShaderResourceView(textureResources[i], &srvDesc, textureHandles[i].cpu);

		constants.EnvMapSrvIndex = textureHandles[i].index;

		DispatchEquirectangularToCubemap(computeCommandList, constants, cubemapSize);
		d3dx::TransitionResource(parameters.Output2.Buffer(), computeCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	}
	return DownloadTexture2DArray(outCubemaps, cubemapSize, cubemapSize, arraySize, 1, format, true, parameters.Output2.Buffer(), computeCommandList);
}

HRESULT 
PrefilterDiffuseD3D12(const ScratchImage& cubemaps, u32 sampleCount, ScratchImage& prefilteredDiffuse)
{
	TODO_("fix sync");
	const TexMetadata& metadata{ cubemaps.GetMetadata() };
	const u32 arraySize{ (u32)metadata.arraySize };
	const u32 cubemapCount{ arraySize / 6 };
	const u16 mipLevels{ (u16)metadata.mipLevels };
	const DXGI_FORMAT format{ metadata.format };
	const u32 bytesPerPixel{ sizeof(v4) }; // NOTE: assuming format is DXGI_FORMAT_R32G32B32A32_FLOAT
	assert(format == DXGI_FORMAT_R32G32B32A32_FLOAT);
	assert(metadata.IsCubemap() && cubemapCount && (arraySize % 6 == 0));

	const u32 cubemapSize{ PREFILTERED_DIFFUSE_CUBEMAP_SIZE };
	hlsl::content::EnvironmentProcessingConstants constants{};
	constants.CubeMapOutSize = cubemapSize;
	constants.SampleCount = sampleCount;

	const u32 bufferSize{ cubemapSize * cubemapSize * arraySize * bytesPerPixel }; //NOTE: assuming format is DXGI_FORMAT_R32G32B32A32_FLOAT
	D3D12BufferInitInfo initInfo{};
	initInfo.heap = nullptr;
	initInfo.alignment = bytesPerPixel;
	initInfo.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	initInfo.size = bufferSize;
	parameters.Output2 = D3D12Buffer{ initInfo, false };

	DXGraphicsCommandList* const computeCommandList{ core::ComputeCommandList() };

	DXDevice* const device{ core::Device() };
	core::StartCompute();

	DescriptorHandle textureHandle{ CopyImagesToSRV(cubemaps, (u32)metadata.width, (u32)metadata.height, mipLevels, arraySize, format, device, computeCommandList) };

	constants.CubemapsSrvIndex = textureHandle.index;
	constants.CubeMapInSize = metadata.width;

	d3dx::TransitionResource(parameters.Output2.Buffer(), computeCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	DispatchPrefilter(computeCommandList, constants, cubemapSize, true);
	d3dx::TransitionResource(parameters.Output2.Buffer(), computeCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	//}

	return DownloadTexture2DArray(prefilteredDiffuse, cubemapSize, cubemapSize, arraySize, 1, format, true, parameters.Output2.Buffer(), computeCommandList);
}

HRESULT 
PrefilterSpecularD3D12(const ScratchImage& cubemaps, u32 sampleCount, ScratchImage& prefilteredSpecular)
{
	const TexMetadata& metadata{ cubemaps.GetMetadata() };
	const u32 arraySize{ (u32)metadata.arraySize };
	const u32 cubemapCount{ arraySize / 6 };
	const u16 mipLevels{ (u16)metadata.mipLevels };
	const DXGI_FORMAT format{ metadata.format };
	const u32 bytesPerPixel{ sizeof(v4) }; // NOTE: assuming format is DXGI_FORMAT_R32G32B32A32_FLOAT
	assert(format == DXGI_FORMAT_R32G32B32A32_FLOAT);
	assert(metadata.IsCubemap() && cubemapCount && (arraySize % 6 == 0));

	const u32 cubemapSize{ PREFILTERED_SPECULAR_CUBEMAP_SIZE };
	hlsl::content::EnvironmentProcessingConstants constants{};
	constants.CubeMapOutSize = cubemapSize;
	constants.SampleCount = sampleCount;

	DXGraphicsCommandList* const computeCommandList{ core::ComputeCommandList() };

	DXDevice* const device{ core::Device() };
	core::StartCompute();

	DescriptorHandle textureHandle{ CopyImagesToSRV(cubemaps, (u32)metadata.width, (u32)metadata.height, mipLevels, arraySize, format, device, computeCommandList) };

	constants.CubemapsSrvIndex = textureHandle.index;
	constants.CubeMapInSize = metadata.width;


	u32 totalByteSize{ 0 };
	u32 currMipSize{ cubemapSize };
	u32 sizes[ROUGHNESS_MIP_LEVELS]{};
	u32 offsets[ROUGHNESS_MIP_LEVELS]{};
	for (u32 mip{ 0 }; mip < ROUGHNESS_MIP_LEVELS; ++mip)
	{
		offsets[mip] = totalByteSize / bytesPerPixel;
		const u32 faceWidth{ currMipSize >> mip };
		sizes[mip] = faceWidth;
		totalByteSize += faceWidth * faceWidth * bytesPerPixel * arraySize; //NOTE: assuming format is DXGI_FORMAT_R32G32B32A32_FLOAT
	}

	const u32 bufferSize{ totalByteSize };
	D3D12BufferInitInfo initInfo{};
	initInfo.heap = nullptr;
	initInfo.alignment = bytesPerPixel;
	initInfo.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	initInfo.size = bufferSize;
	parameters.Output2 = D3D12Buffer{ initInfo, false };
	d3dx::TransitionResource(parameters.Output2.Buffer(), computeCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	for (u32 mip{ 1 }; mip < ROUGHNESS_MIP_LEVELS; ++mip)
	{
		constants.Roughness = (f32)mip * (1.f / ROUGHNESS_MIP_LEVELS);
		constants.OutOffset = offsets[mip];
		constants.CubeMapOutSize = sizes[mip];
		DispatchPrefilter(computeCommandList, constants, sizes[mip], false);
	}

	d3dx::TransitionResource(parameters.Output2.Buffer(), computeCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

	// create a new scrath to avoid overwriting mip 0
	ScratchImage prefilteredScratch{};
	HRESULT hr{ DownloadTexture2DArray(prefilteredScratch, cubemapSize, cubemapSize, arraySize, ROUGHNESS_MIP_LEVELS, format, true, parameters.Output2.Buffer(), computeCommandList) };
	if (FAILED(hr)) return hr;
	assert(metadata.width == prefilteredScratch.GetMetadata().width);
	// copy mip 0 from the source cubemap
	for (u32 imgIdx{ 0 }; imgIdx < arraySize; ++imgIdx)
	{
		const Image& src{ *cubemaps.GetImage(0, imgIdx, 0) };
		const Image& dst{ *prefilteredScratch.GetImage(0, imgIdx, 0) };
		memcpy(dst.pixels, src.pixels, src.slicePitch);
	}
	prefilteredSpecular = std::move(prefilteredScratch);

	return S_OK;
}
HRESULT 
BrdfIntegrationLutD3D12(u32 sampleCount, ScratchImage& brdfLut)
{
	return E_NOTIMPL;
}
}