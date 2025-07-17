#include "EditorContentManager.h"
#include "Editor/Project/Project.h"
#include "yaml-cpp/yaml.h"
#include <fstream>
#include "Editor/ContentBrowser.h"
#include "AssetImporter.h"
#include <ranges>
#include "Utilities/Logger.h"
#include <unordered_set>

namespace mofu::content::assets {
namespace {

std::unordered_map<AssetHandle, AssetPtr> assetRegistry{};

//TODO: check thread-safeness of this
std::array<Vec<std::pair<AssetHandle, id_t>>, AssetType::Count> assetResourcePairs{};

bool
SerializeRegistry()
{
	const std::filesystem::path& registryPath{ mofu::editor::project::GetAssetRegistryPath() };
	assert(std::filesystem::exists(registryPath) && registryPath.extension() == ".ar");

	YAML::Emitter out;
	{
		out << YAML::BeginMap;

		// write out each asset
		for (const auto& [handle, asset] : assetRegistry)
		{
			out << YAML::Key << handle.id;
			out << YAML::Value << YAML::BeginMap;

			out << YAML::Key << "Type" << YAML::Value << static_cast<u32>(asset->Type);
			out << YAML::Key << "OriginalFilePath" << YAML::Value << asset->OriginalFilePath.string();
			out << YAML::Key << "ImportedFilePath" << YAML::Value << asset->ImportedFilePath.string();

			out << YAML::EndMap;
		}

		out << YAML::EndMap;
	}

	std::ofstream outFile{ registryPath };
	outFile << out.c_str();

	return true;
}

bool
DeserializeRegistry()
{
	const std::filesystem::path& registryPath{ mofu::editor::project::GetAssetRegistryPath() };
	assert(std::filesystem::exists(registryPath) && registryPath.extension() == ".ar");

	YAML::Node data{ YAML::LoadFile(registryPath.string()) };
	assert(data.IsDefined());

	for (const auto& item : data) 
	{
		u64 handleId = item.first.as<u64>();
		const YAML::Node& assetNode = item.second;

		AssetType::type type = static_cast<AssetType::type>(assetNode["Type"].as<u32>());
		std::string origPath = assetNode["OriginalFilePath"].as<std::string>();
		std::string impPath = assetNode["ImportedFilePath"].as<std::string>();

		Asset* asset = new Asset{ type, origPath, impPath };
		RegisterAsset(asset, handleId);
	}

	return true;
}

void
RegisterAllAssetsInProject()
{
	std::filesystem::path assetBaseDirectory{ editor::project::GetAssetDirectory() };
	for (const auto& dirEntry : std::filesystem::recursive_directory_iterator{ assetBaseDirectory })
	{
		if (!dirEntry.is_regular_file()) continue;

		const std::filesystem::path& path{ dirEntry.path() };
		std::string extension{ path.extension().string() };

		if (IsEngineAssetExtension(extension)) continue;

		if (IsAllowedAssetExtension(extension) && !IsAssetAlreadyRegistered(path))
		{
			Asset* asset{ new Asset{ GetAssetTypeFromExtension(extension), path, {} } };
			RegisterAsset(asset);
		}
	}
}

} // anonymous namespace

bool
IsAssetAlreadyRegistered(const std::filesystem::path& path)
{
	//FIXME: change in path outside of the project will not be reflected etc
	auto matchedAsset = std::ranges::find_if(assetRegistry, [&](const auto& item) {
			return item.second->OriginalFilePath == path;
		});
	return matchedAsset != assetRegistry.end();
}

bool
IsHandleValid(AssetHandle handle)
{
	return assetRegistry.find(handle) != assetRegistry.end();
}

// shouldn't be called too often, only needed for single item lookups in file browser
AssetHandle 
GetHandleFromPath(const std::filesystem::path& path)
{
	assert(IsAllowedAssetExtension(path.extension().string()));
	assert(!IsEngineAssetExtension(path.extension().string()));
	auto matchedAsset = std::ranges::find_if(assetRegistry, [&](const auto& item) {
		return item.second->OriginalFilePath == path;
		});
	return matchedAsset != assetRegistry.end() ? matchedAsset->first : INVALID_HANDLE;
}

AssetHandle 
GetHandleFromImportedPath(const std::filesystem::path& path)
{
	assert(IsAllowedAssetExtension(path.extension().string()));
	assert(IsEngineAssetExtension(path.extension().string()));
	auto matchedAsset = std::ranges::find_if(assetRegistry, [&](const auto& item) {
		return item.second->ImportedFilePath == path;
		});
	return matchedAsset != assetRegistry.end() ? matchedAsset->first : INVALID_HANDLE;
}

AssetPtr
GetAsset(AssetHandle handle)
{
	if (!IsHandleValid(handle)) return nullptr;

	else if (assetRegistry.find(handle) != assetRegistry.end())
	{
		return assetRegistry.at(handle);
	}

	assert(false);
	return nullptr;
}

// generates a new AssetHandle and puts the asset in the registry
AssetHandle
RegisterAsset(AssetPtr asset)
{
	AssetHandle handle{};
	assetRegistry.emplace(handle, asset);
	editor::AddRegisteredAsset(handle, asset);
	return handle;
}

AssetHandle
RegisterAsset(AssetPtr asset, u64 id)
{
	assert(!GetAsset(id));
	AssetHandle handle{ id };
	assetRegistry.emplace(handle, asset);
	editor::AddRegisteredAsset(handle, asset);
	return handle;
}

void
DeleteAsset(AssetHandle handle)
{
	AssetPtr asset{ GetAsset(handle) };
	editor::DeleteRegisteredAsset(handle);
	if(asset) delete asset;
	assetRegistry.erase(handle);
}

void
PairAssetWithResource(AssetHandle handle, id_t resourceID, AssetType::type type)
{
	assert(id::IsValid(resourceID));

	//TODO: check thread-safeness of this
	assetResourcePairs[type].push_back({ handle, resourceID });
}

AssetHandle
GetAssetFromResource(id_t resourceID, AssetType::type type)
{
	auto set{ assetResourcePairs[type] };
	auto result = std::ranges::find_if(set.begin(), set.end(), [&](const auto& item) {
		return item.second == resourceID;
		});

	if (result != set.end())
	{
		AssetHandle handle{ result->first };
		id_t id{ result->second };
		return handle;
	}
	return INVALID_HANDLE;
}

id_t
GetResourceFromAsset(AssetHandle handle, AssetType::type type)
{
	//TODO: this could have multiple results
	assert(false);
	return id::INVALID_ID;
}

Vec<id_t>
GetResourcesFromAsset(AssetHandle handle, AssetType::type type)
{
	//TODO: this could have multiple results
	assert(false);
	return {};
}

void
ImportAllNotImported()
{
	// for now only textures
	u32 imported{ 0 };
	for (const auto& [handle, asset] : assetRegistry)
	{
		if (imported > 10) return;
		if (std::filesystem::exists(asset->ImportedFilePath)) continue;

		// import
		if (asset->Type == AssetType::Texture)
		{
			ImportAsset(handle);
			imported++;
		}
	}
}

// loads the registry from file
void
InitializeAssetRegistry()
{
	DeserializeRegistry();

	RegisterAllAssetsInProject();
}

// saves the registry to a file
void
ShutdownAssetRegistry()
{
	SerializeRegistry();

	for (auto& [handle, asset] : assetRegistry)
	{
		delete asset;
	}
}

void 
GetTextureIconData(const std::filesystem::path& path, u64& outIconSize, std::unique_ptr<u8[]>& iconBuffer)
{
	assert(std::filesystem::exists(path) && path.extension().string() == ASSET_METADATA_EXTENSION);

	std::ifstream file{ path, std::ios::in | std::ios::binary };
	if (!file)
	{
		log::Error("Failed to open file for reading: %s", path.string().c_str());
		return;
	}

	u32 iconSize{};
	file.read(reinterpret_cast<char*>(&iconSize), sizeof(iconSize));
	assert(iconSize != 0);
	if (iconSize == 0)
	{
		log::Warn("Can't find icon data for texture: %s", path.string().c_str());
	}
	outIconSize = iconSize;
	iconBuffer = std::make_unique<u8[]>(outIconSize);
	file.read(reinterpret_cast<char*>(iconBuffer.get()), iconSize);

	file.close();
}

void 
ParseMetadata(AssetPtr asset)
{
	//TODO:
	switch (asset->Type)
	{
	case content::AssetType::Texture:
	{
		std::filesystem::path metadataPath{ asset->GetMetadataPath() };
		if (std::filesystem::exists(metadataPath) && !id::IsValid(asset->AdditionalData))
		{
			std::unique_ptr<u8[]> iconBuffer{};
			u64 iconSize{};
			content::assets::GetTextureIconData(metadataPath, iconSize, iconBuffer);
			if (iconSize != 0)
			{
				id_t iconId{ graphics::ui::AddIcon(iconBuffer.get()) };
				asset->AdditionalData = iconId;
			}
		}
		break;
	}
	default:
		break;
	}
}

void
GetTextureMetadata(const std::filesystem::path& path, u64& outTextureSize, std::unique_ptr<u8[]>& textureBuffer, 
	u64& outIconSize, std::unique_ptr<u8[]>& iconBuffer)
{
	assert(std::filesystem::exists(path) && path.extension().string().c_str() == ASSET_METADATA_EXTENSION);

	std::ifstream file{ path, std::ios::in | std::ios::binary };
	if (!file)
	{
		log::Error("Failed to open file for reading: %s", path.string().c_str());
		return;
	}

	u32 iconSize{};
	file.read(reinterpret_cast<char*>(&iconSize), sizeof(iconSize));
	assert(iconSize != 0);
	if (iconSize == 0)
	{
		log::Warn("Can't find icon data for texture: %s", path.string().c_str());
	}
	outIconSize = iconSize;
	iconBuffer = std::make_unique<u8[]>(outIconSize);
	file.read(reinterpret_cast<char*>(iconBuffer.get()), iconSize);

	outTextureSize = std::filesystem::file_size(path) - iconSize - sizeof(iconSize);
	assert(outTextureSize != 0);

	textureBuffer = std::make_unique<u8[]>(outTextureSize);
	file.read(reinterpret_cast<char*>(textureBuffer.get()), outTextureSize);

	file.close();
}

}
