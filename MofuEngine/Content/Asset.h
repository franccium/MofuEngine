#pragma once
#include "CommonHeaders.h"
#include "Guid.h"
#include <filesystem>
#include <unordered_set>

namespace mofu::content {
using AssetHandle = Guid;
const AssetHandle INVALID_HANDLE{ AssetHandle(0) };

struct AssetType
{
	enum type : u32
	{
		Unknown = 0,
		Mesh,
		Texture,
		Animation,
		Audio,
		Material,
		Skeleton,

		Count
	};
};

const std::unordered_set<std::string_view> ASSET_EXTENSIONS {
	".mesh", ".tex", ".fbx", ".png", ".tga", ".tiff", ".tif", ".dds", ".hdr", ".jpg", ".jpeg", ".bmp", ".wav"
};

struct Asset
{
	AssetType::type Type;
	std::string Name;
	std::filesystem::path OriginalFilePath;
	std::filesystem::path ImportedFilePath;

	Asset(AssetType::type type, const std::filesystem::path& originalPath, const std::filesystem::path& importedPath)
		: Type{ type }, OriginalFilePath{ originalPath }, ImportedFilePath{ importedPath } 
	{
		Name = OriginalFilePath.stem().string();
	}
};

using AssetPtr = Asset* const;


struct Resource
{
	id_t ID{ id::INVALID_ID };
	void* Data{};
};


const std::unordered_map<std::string_view, AssetType::type> assetTypeFromExtension {
	{ ".mesh", AssetType::Mesh },
	{ ".emesh", AssetType::Mesh },
	{ ".tex", AssetType::Texture },
	{ ".etex", AssetType::Texture },
	{ ".fbx", AssetType::Mesh },
	{ ".png", AssetType::Texture },
	{ ".tga", AssetType::Texture },
	{ ".tiff", AssetType::Texture },
	{ ".tif", AssetType::Texture },
	{ ".dds", AssetType::Texture },
	{ ".hdr", AssetType::Texture },
	{ ".jpg", AssetType::Texture },
	{ ".jpeg", AssetType::Texture },
	{ ".bmp", AssetType::Texture },
	{ ".wav", AssetType::Audio },
};

const std::unordered_map<std::string_view, AssetType::type> assetTypeFromEngineExtension {
	{ ".mesh", AssetType::Mesh },
	{ ".emesh", AssetType::Mesh },
	{ ".tex", AssetType::Texture },
	{ ".etex", AssetType::Texture },
};

[[nodiscard]] inline AssetType::type 
GetAssetTypeFromExtension(std::string_view extension)
{
	if (!assetTypeFromExtension.count(extension))
		return AssetType::Unknown;
	return assetTypeFromExtension.at(extension);
}

[[nodiscard]] inline bool IsAllowedAssetExtension(std::string_view extension)
{
	return ASSET_EXTENSIONS.contains(extension);
}

}