#include "EditorContentManager.h"
#include "Editor/Project/Project.h"
#include "yaml-cpp/yaml.h"
#include <fstream>
#include "Editor/ContentBrowser.h"
#include <ranges>

namespace mofu::content::assets {
namespace {

std::unordered_map<AssetHandle, AssetPtr> assetRegistry{};

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

			out << YAML::Key << "Type" << YAML::Value << static_cast<int>(asset->Type);
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
	editor::AddNewRegisteredAsset(handle);
	return handle;
}

void
DeleteAsset(AssetHandle handle)
{
	assetRegistry.erase(handle);
	editor::DeleteRegisteredAsset(handle);
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

}
