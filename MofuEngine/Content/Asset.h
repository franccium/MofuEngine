#pragma once
#include "CommonHeaders.h"
#include "Guid.h"
#include <filesystem>
#include <unordered_set>
#include <array>

namespace mofu::content {
using AssetHandle = Guid;
const AssetHandle INVALID_HANDLE{ AssetHandle(0) };
inline bool IsValid(AssetHandle handle) { return handle != INVALID_HANDLE; }

struct AssetType
{
	enum type : u32
	{
		Unknown = 0,
		Mesh,
		Texture,
		PhysicsShape,
		Animation,
		Audio,
		Material,
		Shader,
		Skeleton,

		Count
	};
};

constexpr const char* ASSET_TYPE_TO_STRING[content::AssetType::Count] {
	"Unknown",
	"Mesh",
	"Texture",
	"PhysicsShape",
	"Animation",
	"Audio",
	"Material",
	"Shader",
	"Skeleton",
};

constexpr const char* ASSET_METADATA_EXTENSION{ ".mt" };

const std::unordered_set<std::string_view> ASSET_EXTENSIONS {
	".mesh", ".tex", ".fbx", ".png", ".tga", ".tiff", ".tif", ".dds", ".hdr", ".jpg", ".jpeg", ".bmp", ".ps", ".wav", ".mat", ".sd", ".hlsl"
};

const std::unordered_set<std::string_view> ENGINE_ASSET_EXTENSIONS {
	".mesh", ".tex", ".ps", ".mat", ".sd", 
};

struct AssetFlag
{
	enum Flag : u32
	{
		IsBRDFLut,

		Count
	};
};

struct Asset
{
	AssetType::type Type;
	std::string Name;
	std::filesystem::path OriginalFilePath;
	std::filesystem::path ImportedFilePath;
	id_t AdditionalData{ id::INVALID_ID };
	union
	{
		u32 RelatedCount; // for textures, its the icon, for geometry, it determines whether it has metadata
		u32 AdditionalData2;
	};

	std::filesystem::path GetMetadataPath() const { std::filesystem::path p{ ImportedFilePath }; return p.replace_extension(".mt"); }

	Asset(AssetType::type type, const std::filesystem::path& originalPath, const std::filesystem::path& importedPath)
		: Type{ type }, OriginalFilePath{ originalPath }, ImportedFilePath{ importedPath }, RelatedCount{ 0 }
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
	{ ".ps", AssetType::PhysicsShape },
	{ ".wav", AssetType::Audio },
	{ ".mat", AssetType::Material },
	{ ".sd", AssetType::Shader },
	{ ".hlsl", AssetType::Shader},
};

const std::unordered_map<std::string_view, AssetType::type> assetTypeFromEngineExtension {
	{ ".mesh", AssetType::Mesh },
	{ ".tex", AssetType::Texture },
	{ ".ps", AssetType::PhysicsShape },
	{ ".mat", AssetType::Material },
	{ ".sd", AssetType::Shader },
};

constexpr std::array<const char*, AssetType::Count> EXTENSION_FOR_ENGINE_ASSET {
	".mt",
	".mesh",
	".tex",
	".ps",
	".anim",
	".aud",
	".mat",
	".sd",
	".sk",
};

constexpr std::array GET_ALLOWED_TEXTURE_EXTENSIONS{ ".png", ".jpg", ".jpeg", ".bmp", ".tiff", ".tif", ".dds", ".hdr" };
constexpr bool IsAllowedTextureExtension(std::string_view e) 
{ 
	for (const char* c : GET_ALLOWED_TEXTURE_EXTENSIONS)
	{
		if (e == c) return true;
	}
	return false;
}

constexpr const char* PREFAB_FILE_EXTENSION{ ".pre" };
constexpr const char* SCENE_FILE_EXTENSION{ ".sc" };

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

[[nodiscard]] inline bool IsEngineAssetExtension(std::string_view extension)
{
	return ENGINE_ASSET_EXTENSIONS.contains(extension);
}

}