#pragma once
#include "CommonHeaders.h"
#include "Utilities/IOStream.h"
#include <filesystem>
#include "Graphics/GeometryData.h"
#include "PrimitiveMeshGeneration.h"

namespace mofu::content {
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

constexpr const char* ASSET_BASE_DIRECTORY_PATH{ "Assets" };
constexpr const char* GEOMETRY_ASSET_FILE_EXTENSION{ ".geom" };
constexpr const char* GEOMETRY_SERIALIZED_ASSET_VERSION{ "1.0" };
constexpr const char* TEXTURE_SERIALIZED_ASSET_VERSION{ "1.0" };
constexpr const char* ANIMATION_SERIALIZED_ASSET_VERSION{ "1.0" };
constexpr const char* AUDIO_SERIALIZED_ASSET_VERSION{ "1.0" };
constexpr const char* MATERIAL_SERIALIZED_ASSET_VERSION{ "1.0" };
constexpr const char* SKELETON_SERIALIZED_ASSET_VERSION{ "1.0" };
constexpr u32 SERIALIZED_ASSET_VERSION_LENGTH{ (u32)std::char_traits<char>::length(GEOMETRY_SERIALIZED_ASSET_VERSION) };

constexpr const char* ASSET_SERIALIZED_FILE_VERSIONS[AssetType::Count]
{
	"1.0", // unknown asset type
	GEOMETRY_SERIALIZED_ASSET_VERSION,
	TEXTURE_SERIALIZED_ASSET_VERSION,
	ANIMATION_SERIALIZED_ASSET_VERSION,
	AUDIO_SERIALIZED_ASSET_VERSION,
	MATERIAL_SERIALIZED_ASSET_VERSION,
	SKELETON_SERIALIZED_ASSET_VERSION
};

bool LoadEngineShaders(std::unique_ptr<u8[]>& shaders, u64& size);

void ReadAssetFile(std::filesystem::path path, std::unique_ptr<u8[]>& dataOut, u64& sizeOut, AssetType::type type);
void ReadAssetFileNoVersion(std::filesystem::path path, std::unique_ptr<u8[]>& dataOut, u64& sizeOut, AssetType::type type);

void GeneratePrimitiveMeshAsset(PrimitiveMeshInfo info);
void SaveGeometry(const MeshGroupData& data, std::filesystem::path path);

}