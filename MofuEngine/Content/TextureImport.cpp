#include "TextureImport.h"
#include "Content/ResourceCreation.h"
#include "Utilities/IOStream.h"
#include <DirectXTex.h>
#include "NormalMapProcessing.h"
#include "EnvironmentMapProcessing.h"
#include "D3D12EnvironmentMapProcessing.h"

using namespace DirectX;
using namespace Microsoft::WRL;

namespace mofu::content::texture {

namespace {

struct D3D11Device
{
	ComPtr<ID3D11Device> Device;
	std::mutex HwCompressionMutex;
};

std::mutex deviceCreationMutex;
// devices used for BC6 and BC7 compression
Vec<D3D11Device> d3d11Devices;
HMODULE dxgiModule{ nullptr };
HMODULE d3d11Module{ nullptr };

constexpr u32 SAMPLE_COUNT_RANDOM{ 1024 };

constexpr u32
GetMaxMipCount(u32 width, u32 height, u32 depth)
{
	u32 mipLevels{ 0 };
	while (width > 1 || height > 1 || depth > 1)
	{
		width >>= 1;
		height >>= 1;
		depth >>= 1;
		++mipLevels;
	}

	return mipLevels;
}

void
CopySubresources(const ScratchImage& scratch, TextureData* const data)
{
	const Image* const images{ scratch.GetImages() };
	const u32 imageCount{ (u32)scratch.GetImageCount() };
	assert(images && scratch.GetMetadata().mipLevels && scratch.GetMetadata().mipLevels <= TextureData::MAX_MIPS);

	u64 subresourceSize{ 0 };
	for (u32 i{ 0 }; i < imageCount; ++i)
	{
		// 4 x u32 for width, height, rowPitch and slicePitch + slicePitch bytes
		subresourceSize += sizeof(u32) * 4 + images[i].slicePitch;
	}

	if (subresourceSize > UINT_MAX)
	{
		// we support up to 4GB per resource
		data->Info.ImportError = ImportError::MaxSizeExceeded;
		return;
	}

	data->SubresourceSize = (u32)subresourceSize;
	data->SubresourceData = (u8* const)realloc(data->SubresourceData, subresourceSize);
	assert(data->SubresourceData);

	util::BlobStreamWriter blob{ data->SubresourceData, data->SubresourceSize };
	for (u32 i{ 0 }; i < imageCount; ++i)
	{
		const Image& image{ images[i] };
		blob.Write((u32)image.width);
		blob.Write((u32)image.height);
		blob.Write((u32)image.rowPitch);
		blob.Write((u32)image.slicePitch);
		blob.WriteBytes(image.pixels, image.slicePitch);
	}
}

bool
IsHdr(DXGI_FORMAT outputFormat)
{
	switch (outputFormat)
	{
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32_FLOAT:
		return true;
	}

	return false;
}

constexpr void
SetOrClearFlag(u32& flags, u32 flag, bool set)
{
	if (set) flags |= flag; else flags &= ~flag;
}

void
TextureInfoFromMetadata(const TexMetadata& metadata, TextureInfo& info)
{
	const DXGI_FORMAT format{ metadata.format };
	info.Format = format;
	info.Width = (u32)metadata.width;
	info.Height = (u32)metadata.height;
	info.ArraySize = metadata.IsVolumemap() ? (u32)metadata.depth : (u32)metadata.arraySize;
	info.MipLevels = (u32)metadata.mipLevels;
	SetOrClearFlag(info.Flags, TextureFlags::HasAlpha, HasAlpha(format));
	SetOrClearFlag(info.Flags, TextureFlags::IsHdr, IsHdr(format));
	SetOrClearFlag(info.Flags, TextureFlags::IsPremultipliedAlpha, metadata.IsPMAlpha());
	SetOrClearFlag(info.Flags, TextureFlags::IsCubeMap, metadata.IsCubemap());
	SetOrClearFlag(info.Flags, TextureFlags::IsVolumeMap, metadata.IsVolumemap());
	SetOrClearFlag(info.Flags, TextureFlags::IsSrgb, IsSRGB(format));
}

DXGI_FORMAT
DetermineOutputFormat(TextureData* const data, ScratchImage& scratch, const Image* const image)
{
	assert(data && data->ImportSettings.Compress);
	const DXGI_FORMAT imageFormat{ image->format };
	DXGI_FORMAT outputFormat{ (DXGI_FORMAT)data->ImportSettings.OutputFormat };

	if (outputFormat != DXGI_FORMAT_UNKNOWN)
	{
		goto Done;
	}
	if ((data->Info.Flags & TextureFlags::IsHdr)
		|| imageFormat == DXGI_FORMAT_BC6H_UF16 || imageFormat == DXGI_FORMAT_BC6H_SF16)
	{
		outputFormat = DXGI_FORMAT_BC6H_UF16;
	}
	// if the source image is gray scale or a single channel block compressed format (BC4) then output format will be BC4
	else if (imageFormat == DXGI_FORMAT_R8_UNORM || imageFormat == DXGI_FORMAT_BC4_UNORM || imageFormat == DXGI_FORMAT_BC4_SNORM)
	{
		outputFormat = DXGI_FORMAT_BC4_UNORM;
	}
	// if the source image is a normal map, use BC5 format
	else if (IsNormalMap(image) || imageFormat == DXGI_FORMAT_BC5_UNORM || imageFormat == DXGI_FORMAT_BC5_SNORM)
	{
		data->Info.Flags |= TextureFlags::IsImportedAsNormalMap;
		outputFormat = DXGI_FORMAT_BC5_UNORM;

		if (IsSRGB(imageFormat))
		{
			scratch.OverrideFormat(MakeTypelessUNORM(MakeTypeless(imageFormat)));
		}
	}
	else
	{
		outputFormat = data->ImportSettings.PreferBC7 ? DXGI_FORMAT_BC7_UNORM
			: scratch.IsAlphaAllOpaque() ? DXGI_FORMAT_BC1_UNORM : DXGI_FORMAT_BC3_UNORM;
	}

Done:
	assert(IsCompressed(outputFormat));

	if (HasAlpha(outputFormat))
		data->Info.Flags |= TextureFlags::HasAlpha;

	return IsSRGB(imageFormat) ? MakeSRGB(outputFormat) : outputFormat;
}


[[nodiscard]] Vec<Image>
SubresourceDataToImages(TextureData* const data)
{
	assert(data && data->SubresourceData && data->SubresourceSize);
	assert(data->Info.MipLevels && data->Info.MipLevels <= TextureData::MAX_MIPS && data->Info.ArraySize);

	const TextureInfo& info{ data->Info };
	u32 imageCount{ info.ArraySize };

	if (info.Flags & content::TextureFlags::IsVolumeMap)
	{
		u32 depthPerMipLevel{ info.ArraySize };
		for (u32 i{ 0 }; i < info.MipLevels; ++i)
		{
			depthPerMipLevel = std::max(depthPerMipLevel >> 1, (u32)1);
			imageCount += depthPerMipLevel;
		}
	}
	else
	{
		imageCount *= info.MipLevels;
	}

	util::BlobStreamReader blob{ data->SubresourceData };
	Vec<Image> images(imageCount);

	for (u32 i{ 0 }; i < imageCount; ++i)
	{
		Image image{};
		image.width = blob.Read<u32>();
		image.height = blob.Read<u32>();
		image.format = (DXGI_FORMAT)info.Format;
		image.rowPitch = blob.Read<u32>();
		image.slicePitch = blob.Read<u32>();
		image.pixels = (u8*)blob.Position();

		blob.Skip(image.slicePitch);
		images[i] = image;
	}

	return images;
}


Vec<ComPtr<IDXGIAdapter>>
GetAdaptersByPerformance()
{
	if (!dxgiModule)
	{
		dxgiModule = LoadLibrary(L"dxgi.dll");
		if (!dxgiModule) return {};
	}

	using PFNCreateDXGI_Factory1 = HRESULT(WINAPI*)(REFIID, void**);
	const PFNCreateDXGI_Factory1 createDxgiFactory1{ (PFNCreateDXGI_Factory1)GetProcAddress(dxgiModule, "CreateDXGIFactory1") };
	if (!createDxgiFactory1) return {};

	ComPtr<IDXGIFactory7> factory;
	Vec<ComPtr<IDXGIAdapter>> adapters;

	if (SUCCEEDED(createDxgiFactory1(IID_PPV_ARGS(factory.GetAddressOf()))))
	{
		constexpr u32 warpId{ 0x1414 };

		ComPtr<IDXGIAdapter> adapter;
		for (u32 i{ 0 }; factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			if (!adapter) continue;
			DXGI_ADAPTER_DESC desc;
			adapter->GetDesc(&desc);

			if (desc.VendorId != warpId) adapters.emplace_back(adapter);

			adapter.Reset();
		}
	}

	return adapters;
}

void
CreateDevice()
{
	if (d3d11Devices.size() != 0) return;

	if (!d3d11Module)
	{
		d3d11Module = LoadLibrary(L"d3d11.dll");
		if (!d3d11Module) return;
	}

	const PFN_D3D11_CREATE_DEVICE d3d11CreateDevice{ (PFN_D3D11_CREATE_DEVICE)((void*)GetProcAddress(d3d11Module, "D3D11CreateDevice")) };
	if (!d3d11CreateDevice) return;

	u32 createDeviceFlags{ 0 };
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	Vec<ComPtr<IDXGIAdapter>> adapters{ GetAdaptersByPerformance() };
	Vec<ComPtr<ID3D11Device>> devices(adapters.size(), nullptr);
	constexpr D3D_FEATURE_LEVEL featureLevels[]{ D3D_FEATURE_LEVEL_11_1 };

	for (u32 i{ 0 }; i < adapters.size(); ++i)
	{
		ID3D11Device** device{ &devices[i] };
		D3D_FEATURE_LEVEL featureLevel;
		[[maybe_unused]] HRESULT hr{ d3d11CreateDevice(adapters[i].Get(), adapters[i] ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
			nullptr, createDeviceFlags, featureLevels, _countof(featureLevels), D3D11_SDK_VERSION, device, &featureLevel, nullptr) };

		assert(SUCCEEDED(hr));
	}

	for (u32 i{ 0 }; i < devices.size(); ++i)
	{
		// check for valid devices in case creation failed due to feature level not supported
		if (devices[i])
		{
			d3d11Devices.emplace_back();
			d3d11Devices.back().Device = devices[i];
		}
	}
}


bool
TryCreateDevice()
{
	std::lock_guard	lock{ deviceCreationMutex };
	static bool tryCreateDevice = false;
	if (!tryCreateDevice)
	{
		tryCreateDevice = true;
		CreateDevice();
	}

	return d3d11Devices.size() > 0;
}

template<typename T> bool
RunOnGpu(T func)
{
	if (!TryCreateDevice()) return false;

	bool wait{ true };
	bool result{ false };
	while (wait)
	{
		for (u32 i{ 0 }; i < d3d11Devices.size(); ++i)
		{
			if (d3d11Devices[i].HwCompressionMutex.try_lock())
			{
				result = func(d3d11Devices[i].Device.Get());
				d3d11Devices[i].HwCompressionMutex.unlock();
				return result;
			}
		}
		if (wait) std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	return false;
}

bool
CanUseGpuForCompression(DXGI_FORMAT outputFormat)
{
	switch (outputFormat)
	{
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		return true;
	}

	return false;
}

[[nodiscard]] ScratchImage
CompressImage(TextureData* const data, ScratchImage& scratch)
{
	assert(data && data->ImportSettings.Compress && scratch.GetImages());
	const Image* const image{ scratch.GetImage(0, 0, 0) };
	if (!image)
	{
		data->Info.ImportError = ImportError::Unknown;
		return {};
	}

	const DXGI_FORMAT outputFormat{ DetermineOutputFormat(data, scratch, image) };
	HRESULT hr{ S_OK };
	ScratchImage bcScratch;
	if (!(CanUseGpuForCompression(outputFormat)
		&& RunOnGpu([&](ID3D11Device* device)
			{
				hr = Compress(device, scratch.GetImages(), scratch.GetImageCount(), scratch.GetMetadata(),
					outputFormat, TEX_COMPRESS_DEFAULT, 1.0f, bcScratch);
				return SUCCEEDED(hr);
			})))
	{
		// compress on the cpu, if couldn't do it on the gpu
		hr = Compress(scratch.GetImages(), scratch.GetImageCount(), scratch.GetMetadata(),
			outputFormat, TEX_COMPRESS_PARALLEL, data->ImportSettings.AlphaThreshold, bcScratch);
	}

	if (FAILED(hr))
	{
		data->Info.ImportError = ImportError::Compress;
		return {};
	}

	return bcScratch;
}

void
CopyIcon(const Image& image, TextureData* const data)
{
	//ScratchImage scratch;
	//if (FAILED(Decompress(bcImage, DXGI_FORMAT_UNKNOWN, scratch)))
		//return;
	//assert(scratch.GetImages());

	//const Image& image{ scratch.GetImages()[0] };
	// 4 x u32 for width, height, rowPitch and slicePitch + slicePitch bytes
	data->IconSize = (u32)(sizeof(u32) * 5 + image.slicePitch);
	data->Icon = new u8[data->IconSize];
	assert(data->Icon);
	util::BlobStreamWriter blob{ data->Icon, data->IconSize };
	blob.Write((u32)image.width);
	blob.Write((u32)image.height);
	blob.Write((u32)image.format);
	blob.Write((u32)image.rowPitch);
	blob.Write((u32)image.slicePitch);
	blob.WriteBytes(image.pixels, image.slicePitch);
}


void
PrefilterIbl(TextureData* const data, IblFilter::Type filterType)
{
	assert(data->ImportSettings.PrefilterCubemap);
	TextureInfo& info{ data->Info };
	assert(info.Flags & content::TextureFlags::IsCubeMap);
	assert(info.Width == info.Height);
	const DXGI_FORMAT format{ (DXGI_FORMAT)info.Format };
	assert(!IsCompressed(format));
	Vec<Image> images = SubresourceDataToImages(data);
	assert(!images.empty() && !IsCompressed(images[0].format));
	const u32 cubemapCount{ info.ArraySize / 6 };
	assert(info.MipLevels == (u8)(math::log2(info.Width) + 1));
	HRESULT hr{ S_OK };

	ScratchImage cubemaps{};
	hr = cubemaps.InitializeCube(format, info.Width, info.Height, cubemapCount, info.MipLevels);
	if (FAILED(hr))
	{
		info.ImportError = ImportError::Unknown;
		return;
	}

	for (u32 imgIdx{ 0 }; imgIdx < cubemaps.GetImageCount(); ++imgIdx)
	{
		const Image& image{ cubemaps.GetImages()[imgIdx] };
		assert(image.slicePitch == images[imgIdx].slicePitch);
		memcpy(image.pixels, images[imgIdx].pixels, image.slicePitch);
	}

	if (!RunOnGpu([&](ID3D11Device* device)
		{
			hr = filterType == IblFilter::Diffuse
				? PrefilterDiffuse(cubemaps, SAMPLE_COUNT_RANDOM, cubemaps, device)
				: PrefilterSpecular(cubemaps, SAMPLE_COUNT_RANDOM, cubemaps, device);
			return SUCCEEDED(hr);
		}))
	{
		data->Info.ImportError = ImportError::Unknown;
		return;
	}

	if (data->ImportSettings.Compress)
	{
		ScratchImage bcScratch{ CompressImage(data, cubemaps) };
		if (data->Info.ImportError != ImportError::Succeeded) return;

		// decompress the first image to be used in the editor as an icon
		assert(bcScratch.GetImages());
		//CopyIcon(bcScratch.GetImages()[0], data);

		cubemaps = std::move(bcScratch);
	}

	CopySubresources(cubemaps, data);
	TextureInfoFromMetadata(cubemaps.GetMetadata(), data->Info);
	CopyIcon(cubemaps.GetImages()[data->Info.MipLevels / 2], data);
}

[[nodiscard]] ScratchImage
LoadFromBytes(TextureData* data, const u8* const bytes, u32 size)
{
	data->Info.ImportError = ImportError::Load;
	WIC_FLAGS wicFlags{ WIC_FLAGS_NONE };
	TGA_FLAGS tgaFlags{ TGA_FLAGS_NONE };

	if (data->ImportSettings.OutputFormat == DXGI_FORMAT_BC4_UNORM
		|| data->ImportSettings.OutputFormat == DXGI_FORMAT_BC5_SNORM)
	{
		wicFlags |= WIC_FLAGS_IGNORE_SRGB;
		tgaFlags |= TGA_FLAGS_IGNORE_SRGB;
	}

	ScratchImage scratch;

	// try one of WIC formats first (BPM, JPEG, PNG, etc.)
	wicFlags |= WIC_FLAGS_FORCE_RGB;
	HRESULT hr{ LoadFromWICMemory(bytes, size, wicFlags, nullptr, scratch) };

	// if it wasn't a WIC format, try TGA
	if (FAILED(hr))
	{
		hr = LoadFromTGAMemory(bytes, size, tgaFlags, nullptr, scratch);
	}

	// if not TGA try HDR format
	if (FAILED(hr))
	{
		hr = LoadFromHDRMemory(bytes, size, nullptr, scratch);
		if (SUCCEEDED(hr)) data->Info.Flags |= TextureFlags::IsHdr;
	}

	// if not HDR try DDS
	if (FAILED(hr))
	{
		hr = LoadFromDDSMemory(bytes, size, DDS_FLAGS_FORCE_RGB, nullptr, scratch);
		{
			if (SUCCEEDED(hr))
			{
				data->Info.ImportError = ImportError::Decompress;
				ScratchImage mipScratch;
				hr = Decompress(scratch.GetImages(), scratch.GetImageCount(), scratch.GetMetadata(), DXGI_FORMAT_UNKNOWN, mipScratch);

				if (SUCCEEDED(hr))
				{
					scratch = std::move(mipScratch);
				}
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		data->Info.ImportError = ImportError::Succeeded;
	}

	return scratch;
}

[[nodiscard]] ScratchImage
LoadFromFile(TextureData* const data, const char* fileName)
{
	assert(std::filesystem::exists(fileName));
	if (!std::filesystem::exists(fileName))
	{
		data->Info.ImportError = ImportError::FileNotFound;
		return {};
	}

	data->Info.ImportError = ImportError::Load;
	WIC_FLAGS wicFlags{ WIC_FLAGS_NONE };
	TGA_FLAGS tgaFlags{ TGA_FLAGS_NONE };

	if (data->ImportSettings.OutputFormat == DXGI_FORMAT_BC4_UNORM
		|| data->ImportSettings.OutputFormat == DXGI_FORMAT_BC5_SNORM)
	{
		wicFlags |= WIC_FLAGS_IGNORE_SRGB;
		tgaFlags |= TGA_FLAGS_IGNORE_SRGB;
	}

	const std::wstring wfile{ toWstring(fileName) };
	const wchar_t* const file{ wfile.c_str() };
	ScratchImage scratch;

	// try one of WIC formats first (BPM, JPEG, PNG, etc.)
	wicFlags |= WIC_FLAGS_FORCE_RGB;
	HRESULT hr{ LoadFromWICFile(file, wicFlags, nullptr, scratch) };

	// if it wasn't a WIC format, try TGA
	if (FAILED(hr))
	{
		hr = LoadFromTGAFile(file, tgaFlags, nullptr, scratch);
	}

	// if not TGA try HDR format
	if (FAILED(hr))
	{
		hr = LoadFromHDRFile(file, nullptr, scratch);
		if (SUCCEEDED(hr)) data->Info.Flags |= TextureFlags::IsHdr;
	}

	// if not HDR try DDS
	if (FAILED(hr))
	{
		hr = LoadFromDDSFile(file, DDS_FLAGS_FORCE_RGB, nullptr, scratch);
		{
			if (SUCCEEDED(hr))
			{
				data->Info.ImportError = ImportError::Decompress;
				ScratchImage mipScratch;
				hr = Decompress(scratch.GetImages(), scratch.GetImageCount(), scratch.GetMetadata(), DXGI_FORMAT_UNKNOWN, mipScratch);

				if (SUCCEEDED(hr))
				{
					scratch = std::move(mipScratch);
				}
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		data->Info.ImportError = ImportError::Succeeded;
	}

	return scratch;
}

[[nodiscard]] ScratchImage
GenerateMipmaps(const ScratchImage& source, TextureInfo& info, u32 mipLevels, bool is3d)
{
	HRESULT hr{ S_OK };
	ScratchImage mipScratch;
	const TexMetadata& metadata{ source.GetMetadata() };
	mipLevels = math::Clamp(mipLevels, (u32)0, GetMaxMipCount((u32)metadata.width, (u32)metadata.height, (u32)metadata.depth));

	if (!is3d)
	{
		hr = GenerateMipMaps(source.GetImages(), source.GetImageCount(), metadata, TEX_FILTER_DEFAULT, mipLevels, mipScratch);
	}
	else
	{
		hr = GenerateMipMaps3D(source.GetImages(), source.GetImageCount(), metadata, TEX_FILTER_DEFAULT, mipLevels, mipScratch);
	}

	if (FAILED(hr))
	{
		info.ImportError = ImportError::MipmapGeneration;
		return {};
	}

	return mipScratch;
}

[[nodiscard]] ScratchImage
InitializeFromImages(TextureData* const data, const Vec<Image>& images)
{
	assert(data);

	const TextureImportSettings& settings{ data->ImportSettings };
	ScratchImage scratch;
	HRESULT hr{ S_OK };
	const u32 arraySize{ (u32)images.size() };

	{ // scope for working scratch
		ScratchImage workingScratch{};

		if (settings.Dimension == TextureDimension::Texture1D
			|| settings.Dimension == TextureDimension::Texture2D)
		{
			assert(arraySize >= 1);
			const bool allow1d{ settings.Dimension == TextureDimension::Texture1D};
			hr = workingScratch.InitializeArrayFromImages(images.data(), arraySize, allow1d);
		}
		else if (settings.Dimension == TextureDimension::TextureCube)
		{
			const Image& image{ images[0] };
			if (math::IsEqual((f32)image.width / (f32)image.height, 2.f))
			{
				// is an equirectangular image
				//hr = EquirectangularToCubemapD3D12(images.data(), arraySize, settings.CubemapSize,
					//settings.PrefilterCubemap, false, settings.MirrorCubemap, workingScratch);
				if (!RunOnGpu([&](ID3D11Device* device)
					{
						hr = EquirectangularToCubemap(images.data(), arraySize, settings.CubemapSize,
							settings.PrefilterCubemap, false, settings.MirrorCubemap, workingScratch, device);
						return SUCCEEDED(hr);
					}))
				{
					hr = EquirectangularToCubemap(images.data(), arraySize, settings.CubemapSize,
						settings.PrefilterCubemap, false, settings.MirrorCubemap, workingScratch);
				}
			}
			else if (arraySize % 6 != 0 || image.width != image.height)
			{
				data->Info.ImportError = ImportError::NeedSixImages;
				return {};
			}
			else
			{
				hr = workingScratch.InitializeCubeFromImages(images.data(), arraySize);
			}
		}
		else
		{
			assert(settings.Dimension == TextureDimension::Texture3D);
			hr = workingScratch.Initialize3DFromImages(images.data(), arraySize);
		}

		if (FAILED(hr))
		{
			data->Info.ImportError = ImportError::Unknown;
			return {};
		}

		scratch = std::move(workingScratch);
	}

	const bool generateFullMipchain{ settings.PrefilterCubemap && settings.Dimension == TextureDimension::TextureCube };
	if (settings.MipLevels != 1 || generateFullMipchain)
	{
		scratch = GenerateMipmaps(scratch, data->Info,
			generateFullMipchain ? 0 : settings.MipLevels, settings.Dimension == TextureDimension::Texture3D);
	}

	return scratch;
}

[[nodiscard]] ScratchImage
DecompressImage(TextureData* const data)
{
	assert(data->ImportSettings.Compress);

	TextureInfo& info{ data->Info };
	const DXGI_FORMAT format{ (DXGI_FORMAT)info.Format };
	assert(IsCompressed(format));

	Vec<Image> images = SubresourceDataToImages(data);
	const bool is3d{ (info.Flags & TextureFlags::IsVolumeMap) != 0 };

	TexMetadata metadata{};
	metadata.width = info.Width;
	metadata.height = info.Height;
	metadata.format = format;
	metadata.depth = is3d ? info.ArraySize : 1;
	metadata.arraySize = is3d ? 1 : info.ArraySize;
	metadata.mipLevels = info.MipLevels;
	metadata.miscFlags = info.Flags & TextureFlags::IsCubeMap ? TEX_MISC_TEXTURECUBE : 0;
	metadata.miscFlags2 = info.Flags & TextureFlags::IsPremultipliedAlpha ? TEX_ALPHA_MODE_PREMULTIPLIED
		: info.Flags & TextureFlags::HasAlpha ? TEX_ALPHA_MODE_STRAIGHT : TEX_ALPHA_MODE_OPAQUE;
	// TODO: 1D
	metadata.dimension = is3d ? TEX_DIMENSION_TEXTURE3D : TEX_DIMENSION_TEXTURE2D;

	ScratchImage scratch;
	HRESULT hr{ Decompress(images.data(), (size_t)images.size(), metadata, DXGI_FORMAT_UNKNOWN, scratch) };
	if (FAILED(hr))
	{
		info.ImportError = ImportError::Decompress;
		return {};
	}

	return scratch;
}

} // anonymous namespace

void
ShutdownTextureTools()
{
	// release the devices used for compression
	d3d11Devices.clear();

	if (dxgiModule)
	{
		FreeLibrary(dxgiModule);
		dxgiModule = nullptr;
	}

	if (d3d11Module)
	{
		FreeLibrary(d3d11Module);
		d3d11Module = nullptr;
	}
}

void
Decompress(TextureData* const data)
{
	ScratchImage scratch{ DecompressImage(data) };
	if (!data->Info.ImportError)
	{
		CopySubresources(scratch, data);
		TextureInfoFromMetadata(scratch.GetMetadata(), data->Info);
	}
}

void
Import(TextureData* const data)
{
	const TextureImportSettings& settings{ data->ImportSettings };

	assert(!(settings.IsByteArray && (!settings.ImageBytesSize || !settings.ImageBytes || !settings.FileExtension)));
	assert(!(!settings.IsByteArray && settings.Files.empty() && settings.FileCount));

	Vec<ScratchImage> scratchImages;
	Vec<Image> images;

	u32 width{ 0 };
	u32 height{ 0 };
	DXGI_FORMAT format{ DXGI_FORMAT_UNKNOWN };

	if (!settings.IsByteArray)
	{
		Vec<std::string> files = SplitString(settings.Files, ';');
		assert(files.size() == settings.FileCount);

		for (u32 i{ 0 }; i < settings.FileCount; ++i)
		{
			scratchImages.emplace_back(LoadFromFile(data, files[i].c_str()));
		}
	}
	else
	{
		scratchImages.emplace_back(LoadFromBytes(data, data->ImportSettings.ImageBytes, data->ImportSettings.ImageBytesSize));
	}
	if (data->Info.ImportError != ImportError::Succeeded) return;

	bool isFirstScratch{ true };
	for (const ScratchImage& scratch : scratchImages)
	{
		const TexMetadata& metadata{ scratch.GetMetadata() };

		if (isFirstScratch)
		{
			width = (u32)metadata.width;
			height = (u32)metadata.height;
			format = metadata.format;
			isFirstScratch = false;
		}

		// all image sources should have the same size
		if (width != metadata.width || height != metadata.height)
		{
			data->Info.ImportError = ImportError::SizeMismatch;
			return;
		}

		// all image sources should have the same format
		if (format != metadata.format)
		{
			data->Info.ImportError = ImportError::FormatMismatch;
			return;
		}

		const u32 arraySize{ (u32)metadata.arraySize };
		const u32 depth{ (u32)metadata.depth };

		for (u32 arrayIdx{ 0 }; arrayIdx < arraySize; ++arrayIdx)
		{
			for (u32 depthIdx{ 0 }; depthIdx < depth; ++depthIdx)
			{
				const Image* image{ scratch.GetImage(0, arrayIdx, depthIdx) };
				assert(image);

				if (!image)
				{
					data->Info.ImportError = ImportError::Unknown;
					return;
				}

				if (width != image->width || height != image->height)
				{
					data->Info.ImportError = ImportError::SizeMismatch;
					return;
				}

				images.emplace_back(*image);
			}
		}

	}

	ScratchImage scratch{ InitializeFromImages(data, images) };

	if (data->Info.ImportError != ImportError::Succeeded) return;

	// we don't compress if it's a cubemap that will be prefiltered, in that case, the compression is done after prefiltering
	if (settings.Compress && !(scratch.GetMetadata().IsCubemap() && settings.PrefilterCubemap))
	{
		ScratchImage bcScratch{ CompressImage(data, scratch) };
		if (data->Info.ImportError != ImportError::Succeeded) return;

		// decompress the first image to be used in the editor as an icon
		assert(bcScratch.GetImages());
		//CopyIcon(bcScratch.GetImages()[0], data);

		scratch = std::move(bcScratch);
	}

	CopySubresources(scratch, data);
	TextureInfoFromMetadata(scratch.GetMetadata(), data->Info);
	CopyIcon(scratch.GetImages()[data->Info.MipLevels / 2], data);
}

void
PrefilterDiffuseIBL(TextureData* const data)
{
	PrefilterIbl(data, IblFilter::Diffuse);
}

void
PrefilterSpecularIBL(TextureData* const data)
{
	PrefilterIbl(data, IblFilter::Specular);
}

void
ComputeBRDFIntegrationLUT(TextureData* const data)
{
	assert(data);
	constexpr u32 sampleCount{ 1024 };
	HRESULT hr{ S_OK };
	ScratchImage brdfLut{};

	if (!RunOnGpu([&](ID3D11Device* device)
		{
			hr = BrdfIntegrationLut(sampleCount, brdfLut, device);
			return SUCCEEDED(hr);
		}))
	{
		data->Info.ImportError = ImportError::Unknown;
		return;
	}

	CopySubresources(brdfLut, data);
	TextureInfoFromMetadata(brdfLut.GetMetadata(), data->Info);
}

} // anonymous namespace