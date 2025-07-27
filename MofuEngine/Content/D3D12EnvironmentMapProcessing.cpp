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
constexpr u32 PREFILTERED_DIFFUSE_CUBEMAP_SIZE{ 32 };
constexpr u32 PREFILTERED_SPECULAR_CUBEMAP_SIZE{ 32 };
constexpr u32 ROUGHNESS_MIP_LEVELS{ 6 };
constexpr u32 BRDF_INTEGRATION_LUT_SIZE{ 32 };

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
};
static_assert(sizeof(ShaderConstants) == sizeof(hlsl::content::EnvironmentProcessingConstants));

struct EnvironmentProcessingParameters
{
	UAVClearableBuffer Output{};
	D3D12Buffer Output2{};
	ShaderConstants Constants{};
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
			d3dx::D3D12PipelineStateSubobjectCS cs{ shaders::GetContentShader(ContentShader::EnvMapProcessing_EquirectangularToCubeMapCS) };
		} stream;

		equirectangularToCubemapPSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
		NAME_D3D12_OBJECT(equirectangularToCubemapPSO, L"Equirectangular To Cubemap PSO");
	}

	{
		assert(!diffusePrefilterPSO);

		struct
		{
			d3dx::D3D12PipelineStateSubobjectRootSignature rootSig{ environmentProcessingRootSignature };
			d3dx::D3D12PipelineStateSubobjectCS cs{ shaders::GetContentShader(ContentShader::EnvMapProcessing_PrefilterDiffuseEnvMapCS) };
		} stream;

		diffusePrefilterPSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
		NAME_D3D12_OBJECT(diffusePrefilterPSO, L"Diffuse Prefilter PSO");
	}

	{
		assert(!specularPrefilterPSO);

		struct
		{
			d3dx::D3D12PipelineStateSubobjectRootSignature rootSig{ environmentProcessingRootSignature };
			d3dx::D3D12PipelineStateSubobjectCS cs{ shaders::GetContentShader(ContentShader::EnvMapProcessing_PrefilterSpecularEnvMapCS) };
		} stream;

		specularPrefilterPSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
		NAME_D3D12_OBJECT(specularPrefilterPSO, L"Specular Prefilter PSO");
	}

	{
		assert(!computeBrdfLutPSO);

		struct
		{
			d3dx::D3D12PipelineStateSubobjectRootSignature rootSig{ environmentProcessingRootSignature };
			d3dx::D3D12PipelineStateSubobjectCS cs{ shaders::GetContentShader(ContentShader::EnvMapProcessing_ComputeBRDFIntegrationLUTCS) };
		} stream;

		computeBrdfLutPSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
		NAME_D3D12_OBJECT(computeBrdfLutPSO, L"Compute BRDF LUT PSO");
	}

	D3D12BufferInitInfo info = UAVClearableBuffer::GetDefaultInitInfo(1);
	parameters.Output = UAVClearableBuffer{ info };
	NAME_D3D12_OBJECT(parameters.Output.Buffer(), L"Environment Processing Output Buffer");

	return equirectangularToCubemapPSO && diffusePrefilterPSO && specularPrefilterPSO && computeBrdfLutPSO && parameters.Output.Buffer();
}

void
DispatchEquirectangularToCubemap(DXGraphicsCommandList* const cmdList, const ShaderConstants& constants)
{
	assert(constants.EnvMapSrvIndex != 0);
	ConstantBuffer& cbuffer{ core::CBuffer() };
	hlsl::content::EnvironmentProcessingConstants* const buffer{ cbuffer.AllocateSpace<hlsl::content::EnvironmentProcessingConstants>() };
	memcpy(buffer, &constants, sizeof(hlsl::content::EnvironmentProcessingConstants));

	//const u32v4 clearValue{ 0,0,0,0 };
	//parameters.Output.ClearUAV(cmdList, &clearValue.x);
	d3dx::TransitionResource(parameters.Output2.Buffer(), cmdList, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cmdList->SetComputeRootSignature(environmentProcessingRootSignature);
	cmdList->SetPipelineState(equirectangularToCubemapPSO);

	cmdList->SetComputeRootConstantBufferView(CubemapProcessingRootParameter::Constants, cbuffer.GpuAddress(buffer));
	cmdList->SetComputeRootUnorderedAccessView(CubemapProcessingRootParameter::Output, parameters.Output2.GpuAddress());

	cmdList->Dispatch(16, 16, 1);
}

void
DispatchPrefilter(DXGraphicsCommandList* const cmdList, const hlsl::content::EnvironmentProcessingConstants& constants, bool diffuse)
{
	assert(constants.CubemapsSrvIndex != 0);
	ConstantBuffer& cbuffer{ core::CBuffer() };
	hlsl::content::EnvironmentProcessingConstants* const buffer{ cbuffer.AllocateSpace<hlsl::content::EnvironmentProcessingConstants>() };
	memcpy(buffer, &constants, sizeof(hlsl::content::EnvironmentProcessingConstants));

	const u32v4 clearValue{ 0,0,0,0 };
	parameters.Output.ClearUAV(cmdList, &clearValue.x);

	cmdList->SetComputeRootSignature(environmentProcessingRootSignature);
	cmdList->SetPipelineState(diffuse ? diffusePrefilterPSO : specularPrefilterPSO);

	cmdList->SetComputeRootConstantBufferView(0, cbuffer.GpuAddress(buffer));
	cmdList->SetComputeRootUnorderedAccessView(0, parameters.Output.GpuAddress());

	cmdList->Dispatch(16, 16, 1);
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

	//D3D12_RESOURCE_DESC1 desc{ gpuResource->GetDesc1() };
	//u32 subresourceCount{ arraySize * mipLevels };
	//D3D12_PLACED_SUBRESOURCE_FOOTPRINT* const layouts{ (D3D12_PLACED_SUBRESOURCE_FOOTPRINT* const)_alloca(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) * subresourceCount) };
	//u32* const numRows{ (u32* const)_alloca(sizeof(u32) * subresourceCount) };
	//u64* const rowSizes{ (u64* const)_alloca(sizeof(u64) * subresourceCount) };
	//u64 requiredSize{ 0 };
	////DXDevice* device{ core::Device() };
	//device->GetCopyableFootprints1(&desc, 0, subresourceCount, 0, layouts, numRows, rowSizes, &requiredSize);
	//assert(requiredSize && requiredSize < UINT_MAX);

	d3dx::TransitionResource(gpuResource, cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);


	D3D12_RESOURCE_DESC1 readbackBufferDesc{ gpuResource->GetDesc1() };
	readbackBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	readbackBufferDesc.Width = gpuResource->GetDesc1().Width;
	readbackBufferDesc.Height = 1;
	readbackBufferDesc.DepthOrArraySize = 1;
	readbackBufferDesc.MipLevels = 1;
	readbackBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	readbackBufferDesc.SampleDesc.Count = 1;
	readbackBufferDesc.SampleDesc.Quality = 0;
	readbackBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	readbackBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	readbackBufferDesc.SamplerFeedbackMipRegion = {};
	
	DXResource* readbackBuffer;
	DXCall(device->CreateCommittedResource2(&d3dx::HeapProperties.READBACK_HEAP, D3D12_HEAP_FLAG_NONE, &readbackBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, nullptr, IID_PPV_ARGS(&readbackBuffer)));
	cmdList->CopyResource(readbackBuffer, gpuResource);
	
	core::FlushCommandQueue();

	const u32 bufferSize{ (u32)gpuResource->GetDesc1().Width };
	D3D12_RANGE readRange{ 0, bufferSize };
	u8* mappedData{ nullptr };
	DXCall(readbackBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));
	const u32 bytesPerPixel{ sizeof(v4) }; // NOTE: assuming format
	const u32 rowPitch{ width * bytesPerPixel };


	const u32 faceSize{ width * height };
	const u32 faceRowPitch{ width * bytesPerPixel };

	for (u32 arrayIdx{ 0 }; arrayIdx < arraySize; ++arrayIdx) {
		for (u32 mip{ 0 }; mip < mipLevels; ++mip) {
			const Image* img{ outScratch.GetImage(mip, arrayIdx, 0) };
			const u64 faceOffset{ arrayIdx * faceSize * bytesPerPixel };

			u8* src{ mappedData + faceOffset };
			u8* dst{ img->pixels };

			for (u32 row{ 0 }; row < height; ++row) {
				memcpy(dst, src, faceRowPitch);
				src += faceRowPitch;
				dst += img->rowPitch;
			}
		}
	}

	parameters.Output2.Release();
	//TODO: delete the resource
	readbackBuffer->Unmap(0, nullptr);
	core::DeferredRelease(readbackBuffer);

	return hr;
}

HRESULT EquirectangularToCubemapD3D12(const Image* envMaps, u32 envMapCount, u32 cubemapSize,
	bool usePrefilteredSize, bool isSpecular, bool mirrorCubemap, ScratchImage& outCubemaps)
{
	if (usePrefilteredSize) cubemapSize = isSpecular ? PREFILTERED_SPECULAR_CUBEMAP_SIZE : PREFILTERED_DIFFUSE_CUBEMAP_SIZE;
	assert(envMaps && envMapCount);

	const DXGI_FORMAT format{ envMaps[0].format };
	const u32 arraySize{ envMapCount * 6 };

	ShaderConstants constants{};
	constants.CubeMapOutSize = cubemapSize;
	constants.SampleCount = mirrorCubemap ? 1 : 0;

	const u32 bufferSize{ cubemapSize * cubemapSize * arraySize * sizeof(v4) }; //NOTE: assuming format is DXGI_FORMAT_R32G32B32A32_FLOAT
	D3D12BufferInitInfo initInfo{};
	//info.heap = core::UavHeap().Heap();
	initInfo.heap = nullptr;
	initInfo.alignment = sizeof(v4);
	initInfo.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	//initInfo.initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	initInfo.size = bufferSize;
	parameters.Output2 = D3D12Buffer{ initInfo, false };

	/*D3D12_RESOURCE_DESC1 cubeDesc{};
	cubeDesc.Width = cubeDesc.Height = cubemapSize;
	cubeDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	cubeDesc.MipLevels = 1;
	cubeDesc.DepthOrArraySize = arraySize;
	cubeDesc.SampleDesc = { 1,0 };
	cubeDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	cubeDesc.Format = format;
	cubeDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;*/

	//DXDevice* const device{ core::Device() };
	//DXResource* cubemapResource{};
	//DXCall(device->CreateCommittedResource2(&d3dx::HeapProperties.DEFAULT_HEAP, D3D12_HEAP_FLAG_NONE, &cubeDesc,
	//	D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, nullptr, IID_PPV_ARGS(&cubemapResource)));

	//Vec<D3D12_UNORDERED_ACCESS_VIEW_DESC> uavDescs(envMapCount);
	//Vec<DescriptorHandle> uavHandles(envMapCount);

	//for (u32 i{ 0 }; i < envMapCount; ++i)
	//{
	//	uavDescs[i].Format = format;
	//	uavDescs[i].ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	//	uavDescs[i].Texture2DArray.ArraySize = 6;
	//	uavDescs[i].Texture2DArray.FirstArraySlice = i * 6;
	//	uavDescs[i].Texture2DArray.MipSlice = 0;
	//	uavDescs[i].Texture2DArray.PlaneSlice = 0;

	//	uavHandles[i] = core::UavHeap().AllocateDescriptor();
	//	device->CreateUnorderedAccessView(cubemapResource, nullptr, &uavDescs[i], uavHandles[i].cpu);
	//}

	DXDevice* const device{ core::Device() };
	DXGraphicsCommandList* const cmdList{ core::GraphicsCommandList() };
	Vec<DXResource*> textureResources(envMapCount);
	Vec<DescriptorHandle> textureHandles(envMapCount);
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

		// For 2D textures, we only need to copy row by row
		for (u32 row{ 0 }; row < numRows; ++row)
		{
			memcpy(
				dstData + row * layout.Footprint.RowPitch,
				srcData + row * textureData.RowPitch,
				rowSize
			);
		}

		/*D3D12_MEMCPY_DEST copyDest{ cpuAddress + layout.Offset, layout.Footprint.RowPitch, layout.Footprint.RowPitch * numRows };
		for (u32 i{ 0 }; i < numRows; ++i)
		{
			u8* const srcSlice{ (u8* const)textureData.pData + textureData.SlicePitch * i };
			u8* const destSlice{ (u8* const)copyDest.pData + copyDest.SlicePitch * i };

			for (u32 j{ 0 }; j < layout.Footprint.Depth; ++j)
			{
				memcpy(destSlice + copyDest.RowPitch * j, srcSlice + textureData.RowPitch * j, rowSize);
			}
		}*/

		device->CreateCommittedResource2(&d3dx::HeapProperties.DEFAULT_HEAP, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, nullptr, IID_PPV_ARGS(&textureResources[i]));

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

		assert(textureResources[i]);
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;

		textureHandles[i] = core::SrvHeap().AllocateDescriptor();
		device->CreateShaderResourceView(textureResources[i], &srvDesc, textureHandles[i].cpu);

		constants.EnvMapSrvIndex = textureHandles[i].index;

		DispatchEquirectangularToCubemap(cmdList, constants);
	}

	return DownloadTexture2DArray(outCubemaps, cubemapSize, cubemapSize, arraySize, 1, format, true, parameters.Output2.Buffer(), cmdList);
}

HRESULT PrefilterDiffuseD3D12(const ScratchImage& cubemaps, u32 sampleCount, ScratchImage& prefilteredDiffuse)
{
	return E_NOTIMPL;
}
HRESULT PrefilterSpecularD3D12(const ScratchImage& cubemaps, u32 sampleCount, ScratchImage& prefilteredSpecular)
{
	return E_NOTIMPL;
}
HRESULT BrdfIntegrationLutD3D12(u32 sampleCount, ScratchImage& brdfLut)
{
	return E_NOTIMPL;
}
}