#pragma once
#include "CommonHeaders.h"
#include "Content/ResourceCreation.h"
#include <dxgi1_6.h>

namespace mofu::content::texture {
	
struct ImportError
{
	enum ErrorCode : u32
	{
		Succeeded = 0,
		Unknown,
		Compress,
		Decompress,
		Load,
		MipmapGeneration,
		MaxSizeExceeded,
		SizeMismatch,
		FormatMismatch,
		FileNotFound,
		NeedSixImages,

		Count
	};
};

constexpr const char* TEXTURE_IMPORT_ERROR_STRING[ImportError::Count] {
	"None",
	"Unknown",
	"Compress",
	"Decompress",
	"Load",
	"MipmapGeneration",
	"MaxSizeExceeded",
	"SizeMismatch",
	"FormatMismatch",
	"FileNotFound",
	"NeedSixImages",
};

struct IblFilter
{
	enum Type : u32
	{
		Diffuse = 0,
		Specular
	};
};

struct TextureDimension
{
	enum Dimension : u32
	{
		Texture1D,
		Texture2D,
		Texture3D,
		TextureCube,

		Count
	};
};

struct TextureImportSettings
{
	std::string Files{}; // string of one or more file paths separated by ';'
	u32 FileCount{ 0 };
	TextureDimension::Dimension Dimension{ TextureDimension::Texture2D };
	u32 MipLevels{ 0 }; // if 0, the full chain of mip levels is generated
	f32 AlphaThreshold{ 0.f }; // for low-quality block compressed textures with alpha
	DXGI_FORMAT OutputFormat{ DXGI_FORMAT_UNKNOWN };
	u32 CubemapSize{ 256 };
	u8 PreferBC7{ true };
	u8 Compress{ true }; // defines if we should use block compression
	u8 PrefilterCubemap{ true };
	u8 MirrorCubemap{ false };

	bool IsByteArray{ false };
	const char* FileExtension{};
	u32 ImageBytesSize{ 0 };
	const u8* ImageBytes{ nullptr };
};

struct TextureInfo
{
	u32 Width;
	u32 Height;
	u32 ArraySize;
	u32 MipLevels;
	DXGI_FORMAT Format;
	AssetHandle IBLPair{ content::INVALID_HANDLE };
	ImportError::ErrorCode ImportError;
	u32 Flags;
};

struct TextureData
{
	constexpr static u32 MAX_MIPS{ 14 }; // we support up to 8k textures
	u8* SubresourceData{ nullptr };
	u32 SubresourceSize{ 0 };
	u8* Icon{ nullptr }; // uncompressed top mipmap level for the editor
	u32 IconSize{ 0 };
	TextureInfo Info{};
	TextureImportSettings ImportSettings{};
};

void ShutdownTextureTools();

void Decompress(TextureData* const data);

void Import(TextureData* const data);

void PrefilterDiffuseIBL(TextureData* const data); 

void PrefilterSpecularIBL(TextureData* const data);

void ComputeBRDFIntegrationLUT(TextureData* const data);
}