#pragma once
#include "CommonHeaders.h"
#include "yaml-cpp/yaml.h"
#include <filesystem>

namespace mofu::editor::project {
struct ProjectProperties
{
	std::string Name{ "Untitled" };

	std::filesystem::path AssetDirectory{ "Assets" };
	std::filesystem::path ScriptDirectory{ "Scripts" };
	std::filesystem::path ResourceDirectory{ "Resources" };
	std::filesystem::path ResourceMetadataDirectory{ "Metadata" };

	// editable directories where imported assets will be saved to by default
	std::filesystem::path TextureDirectory{ "Textures" };
	std::filesystem::path MeshDirectory{ "Meshes" };
	std::filesystem::path PrefabDirectory{ "Prefabs" };

	u32 StartScene{ 0 };
};

struct Project
{
	ProjectProperties Properties{};
	std::filesystem::path ProjectDirectory;
	std::filesystem::path ProjectFilePath;
	std::filesystem::path AssetRegistryFilePath{ "AssetRegistry.ar" };
};

void CreateNewProject();
void LoadProject(std::filesystem::path projectPath);
void UnloadProject();
void SaveProject();
const Project& GetActiveProject();

void RefreshAllAssets();

const std::filesystem::path GetProjectDirectory();
const std::filesystem::path GetAssetRegistryPath();
const std::filesystem::path GetAssetDirectory();
const std::filesystem::path GetResourceDirectory();
const std::filesystem::path& GetTextureDirectory();
const std::filesystem::path& GetMeshDirectory();
const std::filesystem::path& GetPrefabDirectory();

}