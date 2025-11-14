#include "EnvironmentMapProcessing.h"
#include "ContentUtils.h"
#include "Shaders/ContentProcessingShaders.h"

using namespace DirectX;
using namespace Microsoft::WRL;

namespace mofu::content::texture {
namespace {

constexpr u32 PREFILTERED_DIFFUSE_CUBEMAP_SIZE{ 2048 };
constexpr u32 PREFILTERED_SPECULAR_CUBEMAP_SIZE{ 512 };
constexpr u32 ROUGHNESS_MIP_LEVELS{ 6 };
constexpr u32 BRDF_INTEGRATION_LUT_SIZE{ 512 };

#include "Shaders/out/EnvMapProcessingCS_EquirectangularToCubeMapCS.inc"
#include "Shaders/out/EnvMapProcessingCS_PrefilterDiffuseEnvMapCS.inc"
#include "Shaders/out/EnvMapProcessingCS_PrefilterSpecularEnvMapCS.inc"
#include "Shaders/out/EnvMapProcessingCS_ComputeBRDFIntegrationLUTCS.inc"

struct ShaderConstants
{
	u32 CubeMapInSize;
	u32 CubeMapOutSize;
	u32 SampleCount; // used for prefiltered, but also as a sign to mirror the cubemap when its == 1
	f32 Roughness;
};

v3
GetEquirectangularSampleDirection(u32 face, f32 u, f32 v)
{
	v3 directions[6]{
		{-u, 1.f, -v}, // x+ left
		{u, -1.f, -v}, // x- right
		{v, u, 1.f}, // y+ botton
		{-v, u, -1.f}, // y- top
		{1.f, u, -v}, // z+ front
		{-1.f, -u, -v} // z- back
	};

	XMVECTOR dir{ XMLoadFloat3(&directions[face]) };
	v3 normalizedDir;
	XMStoreFloat3(&normalizedDir, XMVector3Normalize(dir));
	return normalizedDir;
}

v2
DirectionToEquirectangularUV(const v3& dir)
{
	const f32 phi{ atan2f(dir.y, dir.x) };
	const f32 theta{ XMScalarACos(dir.z) };
	const f32 s{ phi * math::INV_TAU + 0.5f };
	const f32 t{ theta * math::INV_PI }; //TODO: confirm

	return { s, t };
}

void
SampleCubeFace(const Image& envMap, const Image& cubeFace, u32 faceIndex, bool mirror)
{
	assert(cubeFace.width == cubeFace.height);
	const f32 invWidth{ 1.f / (f32)cubeFace.width };
	const u32 rowPitch{ (u32)cubeFace.rowPitch };
	const u32 envWidth{ (u32)envMap.width - 1 };
	const u32 envHeight{ (u32)envMap.height - 1 };
	const u32 envRowPitch{ (u32)envMap.rowPitch };

	// asume both envMap and cubeMap are using DXGI_FORMAT_R32G32B32A32_FLOAT
	assert(envMap.format == DXGI_FORMAT_R32G32B32A32_FLOAT && cubeFace.format == DXGI_FORMAT_R32G32B32A32_FLOAT);
	constexpr u32 bytesPerPixel{ 4 * sizeof(f32) };

	for (u32 y{ 0 }; y < cubeFace.width; ++y)
	{
		const f32 v{ (y * invWidth) * 2.f - 1.f };
		for (u32 x{ 0 }; x < cubeFace.width; ++x)
		{
			const f32 u{ (x * invWidth) * 2.f - 1.f };

			const v3 sampleDirection{ GetEquirectangularSampleDirection(faceIndex, u, v) };
			v2 uv{ DirectionToEquirectangularUV(sampleDirection) };
			assert(uv.x >= 0.f && uv.x <= 1.f && uv.y >= 0.f && uv.y <= 1.0f);

			if (mirror) uv.x = 1.f - uv.x;
			const f32 posX{ uv.x * envWidth };
			const f32 posY{ uv.y * envHeight };

			u8* dstPixel{ &cubeFace.pixels[x * bytesPerPixel + y * rowPitch] };
			u8* const srcPixel{ &envMap.pixels[(u32)posX * bytesPerPixel + (u32)posY * envRowPitch] };
			memcpy(dstPixel, srcPixel, bytesPerPixel);
		}
	}
}

void
ResetD3D11Context(ID3D11DeviceContext* const ctx)
{
	u8 zeroMemBlock[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT * sizeof(void*)];
	memset(&zeroMemBlock, 0, sizeof(zeroMemBlock));

	ctx->CSSetUnorderedAccessViews(0, D3D11_1_UAV_SLOT_COUNT, (ID3D11UnorderedAccessView**)&zeroMemBlock[0], nullptr);
	ctx->CSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, (ID3D11ShaderResourceView**)&zeroMemBlock[0]);
	ctx->CSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, (ID3D11Buffer**)&zeroMemBlock[0]);
}

HRESULT
SetShaderConstants(ID3D11DeviceContext* const ctx, ID3D11Buffer* const constantBuffer, ShaderConstants shaderConstants)
{
	D3D11_MAPPED_SUBRESOURCE mappedBuffer{};
	HRESULT hr{ ctx->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedBuffer) };
	if (FAILED(hr) || !mappedBuffer.pData) return hr;

	memcpy(mappedBuffer.pData, &shaderConstants, sizeof(ShaderConstants));
	ctx->Unmap(constantBuffer, 0);
	return hr;
}

HRESULT
CreateConstantBuffer(ID3D11Device* const device, ID3D11Buffer** constantBuffer)
{
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(ShaderConstants);
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;

	return device->CreateBuffer(&desc, nullptr, constantBuffer);
}

HRESULT
CreateLinearSampler(ID3D11Device* const device, ID3D11SamplerState** linearSampler)
{
	D3D11_SAMPLER_DESC desc{};
	desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	desc.MinLOD = 0;
	desc.MaxLOD = D3D11_FLOAT32_MAX;

	return device->CreateSamplerState(&desc, linearSampler);
}

HRESULT
CreateCubemapSRV(ID3D11Device* device, DXGI_FORMAT format, u32 firstArraySlice, u32 mipLevels,
	ID3D11Resource* const cubemaps, ID3D11ShaderResourceView** cubemapSRV)
{
	D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
	desc.Format = format;
	desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE; //TODO:
	desc.TextureCubeArray.NumCubes = 1;
	desc.TextureCubeArray.First2DArrayFace = firstArraySlice;
	desc.TextureCubeArray.MipLevels = mipLevels;

	return device->CreateShaderResourceView(cubemaps, &desc, cubemapSRV);
}

HRESULT
CreateTexture2DArrayUAV(ID3D11Device* device, DXGI_FORMAT format, u32 arraySize, u32 firstArraySlice,
	u32 mipSlice, ID3D11Resource* texture, ID3D11UnorderedAccessView** textureUAV)
{
	D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
	desc.Format = format;
	desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
	desc.Texture2DArray.ArraySize = arraySize;
	desc.Texture2DArray.FirstArraySlice = firstArraySlice;
	desc.Texture2DArray.MipSlice = mipSlice;

	return device->CreateUnorderedAccessView(texture, &desc, textureUAV);
}

HRESULT
DownloadTexture2DArray(ScratchImage& outScratch, u32 width, u32 height, u32 arraySize,  u32 mipLevels, DXGI_FORMAT format, bool isCubemap, ID3D11DeviceContext* const ctx, ID3D11Texture2D* const gpuResource, ID3D11Texture2D* const cpuResource)
{
	ctx->CopyResource(cpuResource, gpuResource);

	HRESULT hr{ isCubemap
		? outScratch.InitializeCube(format, width, height, arraySize / 6, mipLevels)
		: outScratch.Initialize2D(format, width, height, arraySize, mipLevels) };
	if (FAILED(hr)) return hr;


	for (u32 imgIdx{ 0 }; imgIdx < arraySize; ++imgIdx)
	{
		for (u32 mip{ 0 }; mip < mipLevels; ++mip)
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource{};
			const u32 resourceIdx{ mip + (imgIdx * mipLevels) };
			hr = ctx->Map(cpuResource, resourceIdx, D3D11_MAP_READ, 0, &mappedResource);
			if (FAILED(hr))
			{
				outScratch.Release();
				return hr;
			}

			const Image& img{ *outScratch.GetImage(mip, imgIdx, 0) };
			u8* src{ (u8*)mappedResource.pData };
			u8* dst{ img.pixels };

			for (u32 row{ 0 }; row < img.height; ++row)
			{
				memcpy(dst, src, img.rowPitch);
				src += mappedResource.RowPitch;
				dst += img.rowPitch;
			}

			ctx->Unmap(cpuResource, resourceIdx);
		}
	}

	return hr;
}

void
Dispatch(u32v3 groupCount, ID3D11ComputeShader* const shader, ID3D11SamplerState** const samplersArray, ID3D11ShaderResourceView** const srvArray,
	ID3D11UnorderedAccessView** const uavArray, ID3D11Buffer** const buffersArray, ID3D11DeviceContext* const ctx)
{
	ctx->CSSetShaderResources(0, 1, srvArray);
	ctx->CSSetUnorderedAccessViews(0, 1, uavArray, nullptr);
	ctx->CSSetConstantBuffers(0, 1, &buffersArray[0]);
	ctx->CSSetSamplers(0, 1, &samplersArray[0]);
	ctx->CSSetShader(shader, nullptr, 0);
	ctx->Dispatch(groupCount.x, groupCount.y, groupCount.z);
}

} // anonymous namespace

HRESULT
EquirectangularToCubemap(const Image* envMaps, u32 envMapCount, u32 cubemapSize, 
	bool usePrefilteredSize, bool isSpecular, bool mirrorCubemap, ScratchImage& outCubemaps, ID3D11Device* const device)
{
	assert(envMaps && envMapCount);

	const DXGI_FORMAT format{ envMaps[0].format };
	const u32 arraySize{ envMapCount * 6 };
	ComPtr<ID3D11DeviceContext> ctx{};
	device->GetImmediateContext(ctx.GetAddressOf());
	assert(ctx.Get());

	HRESULT hr{ S_OK };

	ComPtr<ID3D11Texture2D> cubemaps{};
	ComPtr<ID3D11Texture2D> cubemapsCPU{};
	if (usePrefilteredSize) cubemapSize = isSpecular ? PREFILTERED_SPECULAR_CUBEMAP_SIZE : PREFILTERED_DIFFUSE_CUBEMAP_SIZE;
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = desc.Height = cubemapSize;
		desc.MipLevels = 1;
		desc.ArraySize = arraySize;
		desc.Format = format;
		desc.SampleDesc = { 1, 0 };
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		hr = device->CreateTexture2D(&desc, nullptr, cubemaps.GetAddressOf());
		if (FAILED(hr))
		{
			return hr;
		}

		desc.BindFlags = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		hr = device->CreateTexture2D(&desc, nullptr, cubemapsCPU.GetAddressOf());
		if (FAILED(hr))
		{
			return hr;
		}
	}

	ComPtr<ID3D11ComputeShader> shader{};
	hr = device->CreateComputeShader(EnvMapProcessingCS_EquirectangularToCubeMapCS,
		sizeof(EnvMapProcessingCS_EquirectangularToCubeMapCS), nullptr, shader.GetAddressOf());
	if (FAILED(hr))
	{
		return hr;
	}

	ComPtr<ID3D11Buffer> constantBuffer{};
	{
		hr = CreateConstantBuffer(device, constantBuffer.GetAddressOf());
		if (FAILED(hr))
		{
			return hr;
		}

		ShaderConstants constants{};
		constants.CubeMapOutSize = cubemapSize;
		constants.SampleCount = mirrorCubemap ? 1 : 0;
		hr = SetShaderConstants(ctx.Get(), constantBuffer.Get(), constants);
		if (FAILED(hr))
		{
			return hr;
		}
	}

	ComPtr<ID3D11SamplerState> linearSampler{};
	hr = CreateLinearSampler(device, linearSampler.GetAddressOf());
	if (FAILED(hr))
	{
		return hr;
	}

	ResetD3D11Context(ctx.Get());

	for (u32 i{ 0 }; i < envMapCount; ++i)
	{
		ComPtr<ID3D11UnorderedAccessView> cubemapUAV{};
		//TODO: returns invalid args for some reason
		//hr = CreateTexture2DArrayUAV(device, envMaps[0].format, 6, i * 6, 0, cubemaps.Get(), cubemapUAV.ReleaseAndGetAddressOf());
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
			desc.Format = format;
			desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.ArraySize = 6;
			desc.Texture2DArray.FirstArraySlice = i * 6;
			desc.Texture2DArray.MipSlice = 0;

			hr = device->CreateUnorderedAccessView(cubemaps.Get(), &desc, cubemapUAV.ReleaseAndGetAddressOf());
			if (FAILED(hr))
			{
				return hr;
			}
		}

		const Image& src{ envMaps[i] };

		// upload source image to GPU
		ComPtr<ID3D11Texture2D> envMap{};
		{
			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = (u32)src.width;
			desc.Height = (u32)src.height;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = format;
			desc.SampleDesc = { 1, 0 };
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;

			D3D11_SUBRESOURCE_DATA data{};
			data.pSysMem = src.pixels;
			data.SysMemPitch = (u32)src.rowPitch;

			hr = device->CreateTexture2D(&desc, &data, envMap.ReleaseAndGetAddressOf());
			if (FAILED(hr))
			{
				return hr;
			}
		}

		ComPtr<ID3D11ShaderResourceView> envMapSRV{};
		hr = device->CreateShaderResourceView(envMap.Get(), nullptr, envMapSRV.ReleaseAndGetAddressOf());
		if (FAILED(hr))
		{
			return hr;
		}

		const u32 blockSize{ (cubemapSize + 15) >> 4 };
		Dispatch({ blockSize, blockSize, 6 }, shader.Get(), linearSampler.GetAddressOf(), envMapSRV.GetAddressOf(), cubemapUAV.GetAddressOf(),
			constantBuffer.GetAddressOf(), ctx.Get());
	}

	ResetD3D11Context(ctx.Get());

	return DownloadTexture2DArray(outCubemaps, cubemapSize, cubemapSize, arraySize, 1,
		format, true, ctx.Get(), cubemaps.Get(), cubemapsCPU.Get());
}

HRESULT
EquirectangularToCubemap(const Image* envMaps, u32 envMapCount, 
	u32 cubemapSize, bool usePrefilteredSize, bool isSpecular, bool mirrorCubemap, ScratchImage& outCubemaps)
{
	if (usePrefilteredSize) cubemapSize = isSpecular ? PREFILTERED_SPECULAR_CUBEMAP_SIZE : PREFILTERED_DIFFUSE_CUBEMAP_SIZE;

	assert(envMaps && envMapCount);
	HRESULT hr{ S_OK };

	// initialize 1 texture cube for each image
	ScratchImage workingScratch{};
	hr = workingScratch.InitializeCube(DXGI_FORMAT_R32G32B32A32_FLOAT, cubemapSize, cubemapSize, envMapCount, 1);
	if (FAILED(hr))
	{
		return hr;
	}

	for (u32 i{ 0 }; i < envMapCount; ++i)
	{
		const Image& envMap{ envMaps[i] };
		assert(math::IsEqual((f32)envMap.width / (f32)envMap.height, 2.f));

		ScratchImage f32EnvMap{};
		if (envMaps[0].format != DXGI_FORMAT_R32G32B32A32_FLOAT)
		{
			hr = Convert(envMap, DXGI_FORMAT_R32G32B32A32_FLOAT, TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, f32EnvMap);
			if (FAILED(hr))
			{
				return hr;
			}
		}
		else
		{
			f32EnvMap.InitializeFromImage(envMap);
		}

		assert(f32EnvMap.GetImageCount() == 1);
		const Image* dstImages{ &workingScratch.GetImages()[i * 6] };
		const Image& envMapImage{ f32EnvMap.GetImages()[i] };

		std::thread threads[]{
			std::thread{ [&] { SampleCubeFace(envMapImage, dstImages[0], 0, mirrorCubemap); } },
			std::thread{ [&] { SampleCubeFace(envMapImage, dstImages[1], 1, mirrorCubemap); } },
			std::thread{ [&] { SampleCubeFace(envMapImage, dstImages[2], 2, mirrorCubemap); } },
			std::thread{ [&] { SampleCubeFace(envMapImage, dstImages[3], 3, mirrorCubemap); } },
			std::thread{ [&] { SampleCubeFace(envMapImage, dstImages[4], 4, mirrorCubemap); } },
		};
		SampleCubeFace(envMapImage, dstImages[5], 5, mirrorCubemap);

		for (u32 face{ 0 }; face < 5; ++face) threads[face].join();
	}

	if (envMaps[0].format != DXGI_FORMAT_R32G32B32A32_FLOAT)
	{
		hr = Convert(workingScratch.GetImages(), workingScratch.GetImageCount(), workingScratch.GetMetadata(),
			envMaps[0].format, TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, outCubemaps);
	}
	else
	{
		outCubemaps = std::move(workingScratch);
	}

	return hr;
}

HRESULT
PrefilterDiffuse(const ScratchImage& cubemaps, u32 sampleCount, ScratchImage& outPrefilteredDiffuse, ID3D11Device* const device)
{
	const TexMetadata& metadata{ cubemaps.GetMetadata() };
	const u32 arraySize{ (u32)metadata.arraySize };
	const u32 cubemapCount{ arraySize / 6 };
	assert(device && metadata.IsCubemap() && cubemapCount && (arraySize % 6 == 0));

	ComPtr<ID3D11DeviceContext> ctx{};
	device->GetImmediateContext(ctx.GetAddressOf());
	assert(ctx.Get());
	HRESULT hr{ S_OK };

	ComPtr<ID3D11Texture2D> cubemapsIN{};
	ComPtr<ID3D11Texture2D> outCubemaps{};
	ComPtr<ID3D11Texture2D> cubemapsCPU{};
	{
		// upload the source cubemap
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = (u32)metadata.width;
		desc.Height = (u32)metadata.height;
		desc.MipLevels = (u32)metadata.mipLevels;
		desc.ArraySize = arraySize;
		desc.Format = metadata.format;
		desc.SampleDesc = { 1, 0 };
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

		{
			const u32 imageCount{ (u32)cubemaps.GetImageCount() };
			const Image* images{ cubemaps.GetImages() };
			Vec<D3D11_SUBRESOURCE_DATA> inputData(imageCount);
			for (u32 i{ 0 }; i < imageCount; ++i)
			{
				inputData[i].pSysMem = images[i].pixels;
				inputData[i].SysMemPitch = (u32)images[i].rowPitch;
			}

			hr = device->CreateTexture2D(&desc, inputData.data(), cubemapsIN.GetAddressOf());
			if (FAILED(hr)) return hr;
		}

		desc.Width = desc.Height = PREFILTERED_DIFFUSE_CUBEMAP_SIZE;
		desc.MipLevels = 1;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		desc.MiscFlags = 0;
		hr = device->CreateTexture2D(&desc, nullptr, outCubemaps.GetAddressOf());
		if (FAILED(hr)) return hr;

		desc.BindFlags = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		hr = device->CreateTexture2D(&desc, nullptr, cubemapsCPU.GetAddressOf());
		if (FAILED(hr)) return hr;
	}

	ComPtr<ID3D11ComputeShader> shader{};
	hr = device->CreateComputeShader(EnvMapProcessingCS_PrefilterDiffuseEnvMapCS,
		sizeof(EnvMapProcessingCS_PrefilterDiffuseEnvMapCS), nullptr, shader.GetAddressOf());
	if (FAILED(hr)) return hr;

	ComPtr<ID3D11Buffer> constantBuffer{};
	{
		hr = CreateConstantBuffer(device, constantBuffer.GetAddressOf());
		if (FAILED(hr)) return hr;

		ShaderConstants constants{};
		constants.CubeMapInSize = (u32)metadata.width;
		constants.CubeMapOutSize = PREFILTERED_DIFFUSE_CUBEMAP_SIZE;
		constants.SampleCount = sampleCount;
		hr = SetShaderConstants(ctx.Get(), constantBuffer.Get(), constants);
		if (FAILED(hr)) return hr;
	}

	ComPtr<ID3D11SamplerState> linearSampler{};
	hr = CreateLinearSampler(device, linearSampler.GetAddressOf());
	if (FAILED(hr)) return hr;

	ResetD3D11Context(ctx.Get());

	for (u32 i{ 0 }; i < cubemapCount; ++i)
	{
		ComPtr<ID3D11ShaderResourceView> cubemapsInSRV{};
		hr = CreateCubemapSRV(device, metadata.format, i * 6, 1, cubemapsIN.Get(), cubemapsInSRV.ReleaseAndGetAddressOf());
		if (FAILED(hr)) return hr;

		ComPtr<ID3D11UnorderedAccessView> outCubemapsUAV{};
		hr = CreateTexture2DArrayUAV(device, metadata.format, 6, i * 6, 0, outCubemaps.Get(), outCubemapsUAV.ReleaseAndGetAddressOf());
		if (FAILED(hr)) return hr;

		const u32 blockSize{ (PREFILTERED_DIFFUSE_CUBEMAP_SIZE + 15) >> 4 };
		Dispatch({ blockSize, blockSize, 6 }, shader.Get(), linearSampler.GetAddressOf(), cubemapsInSRV.GetAddressOf(), outCubemapsUAV.GetAddressOf(),
			constantBuffer.GetAddressOf(), ctx.Get());
	}

	ResetD3D11Context(ctx.Get());

	return DownloadTexture2DArray(outPrefilteredDiffuse, PREFILTERED_DIFFUSE_CUBEMAP_SIZE, PREFILTERED_DIFFUSE_CUBEMAP_SIZE,
		arraySize, 1, metadata.format, true, ctx.Get(), outCubemaps.Get(), cubemapsCPU.Get());
}

HRESULT
PrefilterSpecular(const ScratchImage& cubemaps, u32 sampleCount, ScratchImage& outPrefilteredSpecular, ID3D11Device* const device)
{
	const TexMetadata& metadata{ cubemaps.GetMetadata() };
	const u32 arraySize{ (u32)metadata.arraySize };
	const u32 cubemapCount{ arraySize / 6 };
	assert(device && metadata.IsCubemap() && cubemapCount && (arraySize % 6 == 0));

	ComPtr<ID3D11DeviceContext> ctx{};
	device->GetImmediateContext(ctx.GetAddressOf());
	assert(ctx.Get());
	HRESULT hr{ S_OK };

	ComPtr<ID3D11Texture2D> cubemapsIN{};
	ComPtr<ID3D11Texture2D> outCubemaps{};
	ComPtr<ID3D11Texture2D> cubemapsCPU{};
	{
		// upload the source cubemap
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = (u32)metadata.width;
		desc.Height = (u32)metadata.height;
		desc.MipLevels = (u32)metadata.mipLevels;
		desc.ArraySize = arraySize;
		desc.Format = metadata.format;
		desc.SampleDesc = { 1, 0 };
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

		{
			const u32 imageCount{ (u32)cubemaps.GetImageCount() };
			const Image* images{ cubemaps.GetImages() };
			Vec<D3D11_SUBRESOURCE_DATA> inputData(imageCount);
			for (u32 i{ 0 }; i < imageCount; ++i)
			{
				inputData[i].pSysMem = images[i].pixels;
				inputData[i].SysMemPitch = (u32)images[i].rowPitch;
			}

			hr = device->CreateTexture2D(&desc, inputData.data(), cubemapsIN.GetAddressOf());
			if (FAILED(hr)) return hr;
		}

		desc.Width = desc.Height = PREFILTERED_SPECULAR_CUBEMAP_SIZE;
		desc.MipLevels = ROUGHNESS_MIP_LEVELS;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		desc.MiscFlags = 0;
		hr = device->CreateTexture2D(&desc, nullptr, outCubemaps.GetAddressOf());
		if (FAILED(hr)) return hr;

		desc.BindFlags = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		hr = device->CreateTexture2D(&desc, nullptr, cubemapsCPU.GetAddressOf());
		if (FAILED(hr)) return hr;
	}

	ComPtr<ID3D11ComputeShader> shader{};
	hr = device->CreateComputeShader(EnvMapProcessingCS_PrefilterSpecularEnvMapCS,
		sizeof(EnvMapProcessingCS_PrefilterSpecularEnvMapCS), nullptr, shader.GetAddressOf());
	if (FAILED(hr)) return hr;

	ComPtr<ID3D11Buffer> constantBuffer{};
	{
		hr = CreateConstantBuffer(device, constantBuffer.GetAddressOf());
		if (FAILED(hr)) return hr;
	}

	ComPtr<ID3D11SamplerState> linearSampler{};
	hr = CreateLinearSampler(device, linearSampler.GetAddressOf());
	if (FAILED(hr)) return hr;

	ResetD3D11Context(ctx.Get());

	for (u32 i{ 0 }; i < cubemapCount; ++i)
	{
		ComPtr<ID3D11ShaderResourceView> cubemapsInSRV{};
		hr = CreateCubemapSRV(device, metadata.format, i * 6, (u32)metadata.mipLevels, cubemapsIN.Get(), cubemapsInSRV.ReleaseAndGetAddressOf());
		if (FAILED(hr)) return hr;

		for (u32 mip{ 1 }; mip < ROUGHNESS_MIP_LEVELS; ++mip)
		{
			ComPtr<ID3D11UnorderedAccessView> outCubemapsUAV{};
			hr = CreateTexture2DArrayUAV(device, metadata.format, 6, i * 6, mip, outCubemaps.Get(), outCubemapsUAV.ReleaseAndGetAddressOf());
			if (FAILED(hr)) return hr;

			ShaderConstants constants{};
			constants.CubeMapInSize = (u32)metadata.width;
			constants.CubeMapOutSize = PREFILTERED_SPECULAR_CUBEMAP_SIZE;
			constants.SampleCount = sampleCount;
			constants.Roughness = mip * (1.f / ROUGHNESS_MIP_LEVELS);
			hr = SetShaderConstants(ctx.Get(), constantBuffer.Get(), constants);
			if (FAILED(hr)) return hr;

			const u32 blockSize{ std::max((u32)1, (u32)((PREFILTERED_SPECULAR_CUBEMAP_SIZE >> mip) + 15) >> 4) };
			Dispatch({ blockSize, blockSize, 6 }, shader.Get(), linearSampler.GetAddressOf(), cubemapsInSRV.GetAddressOf(), outCubemapsUAV.GetAddressOf(),
				constantBuffer.GetAddressOf(), ctx.Get());
		}
	}

	ResetD3D11Context(ctx.Get());

	ScratchImage prefilteredScratch{};
	hr = DownloadTexture2DArray(prefilteredScratch, PREFILTERED_SPECULAR_CUBEMAP_SIZE, PREFILTERED_SPECULAR_CUBEMAP_SIZE,
		arraySize, ROUGHNESS_MIP_LEVELS, metadata.format, true, ctx.Get(), outCubemaps.Get(), cubemapsCPU.Get());
	if (FAILED(hr)) return hr;

	assert(metadata.width == prefilteredScratch.GetMetadata().width);
	// copy mip 0 from the source cubemap
	for (u32 imgIdx{ 0 }; imgIdx < arraySize; ++imgIdx)
	{
		const Image& src{ *cubemaps.GetImage(0, imgIdx, 0) };
		const Image& dst{ *prefilteredScratch.GetImage(0, imgIdx, 0) };
		memcpy(dst.pixels, src.pixels, src.slicePitch);
	}

	outPrefilteredSpecular = std::move(prefilteredScratch);
	return hr;
}

HRESULT
BrdfIntegrationLut(u32 sampleCount, ScratchImage& outBrdfLUT, ID3D11Device* const device)
{
	ComPtr<ID3D11DeviceContext> ctx{};
	device->GetImmediateContext(ctx.GetAddressOf());
	assert(ctx.Get());
	HRESULT hr{ S_OK };

	constexpr u32 lutSize{ BRDF_INTEGRATION_LUT_SIZE };
	constexpr DXGI_FORMAT format{ DXGI_FORMAT_R16G16_FLOAT };

	ComPtr<ID3D11Texture2D> output{};
	ComPtr<ID3D11Texture2D> outputCPU{};
	{
		// upload the source cubemap
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = desc.Height = lutSize;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = format;
		desc.SampleDesc = { 1, 0 };
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		hr = device->CreateTexture2D(&desc, nullptr, output.GetAddressOf());
		if (FAILED(hr)) return hr;

		desc.BindFlags = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		hr = device->CreateTexture2D(&desc, nullptr, outputCPU.GetAddressOf());
		if (FAILED(hr)) return hr;
	}

	ComPtr<ID3D11ComputeShader> shader{};
	hr = device->CreateComputeShader(EnvMapProcessingCS_ComputeBRDFIntegrationLUTCS,
		sizeof(EnvMapProcessingCS_ComputeBRDFIntegrationLUTCS), nullptr, shader.GetAddressOf());
	if (FAILED(hr)) return hr;

	ComPtr<ID3D11Buffer> constantBuffer{};
	{
		hr = CreateConstantBuffer(device, constantBuffer.GetAddressOf());
		if (FAILED(hr)) return hr;

		ShaderConstants constants{};
		constants.CubeMapOutSize = lutSize;
		constants.SampleCount = sampleCount;
		hr = SetShaderConstants(ctx.Get(), constantBuffer.Get(), constants);
		if (FAILED(hr)) return hr;
	}

	ComPtr<ID3D11SamplerState> linearSampler{};
	hr = CreateLinearSampler(device, linearSampler.GetAddressOf());
	if (FAILED(hr)) return hr;

	ResetD3D11Context(ctx.Get());

	ComPtr<ID3D11UnorderedAccessView> outputUAV{};
	hr = CreateTexture2DArrayUAV(device, format, 1, 0, 0, output.Get(), outputUAV.ReleaseAndGetAddressOf());
	if (FAILED(hr)) return hr;

	ID3D11ShaderResourceView* srv{ nullptr };
	const u32 blockSize{ (lutSize + 15) >> 4 };
	Dispatch({ blockSize, blockSize, 1 }, shader.Get(), linearSampler.GetAddressOf(), &srv, outputUAV.GetAddressOf(),
		constantBuffer.GetAddressOf(), ctx.Get());

	ResetD3D11Context(ctx.Get());

	return DownloadTexture2DArray(outBrdfLUT, lutSize, lutSize, 1, 1, format, false, ctx.Get(), output.Get(), outputCPU.Get());
}

}