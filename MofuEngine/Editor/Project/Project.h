#pragma once
#include "CommonHeaders.h"
#include "yaml-cpp/yaml.h"
#include <filesystem>

namespace mofu::editor::project {
struct ProjectProperties
{
	std::string Name{ "Untitled" };

	std::filesystem::path AssetDirectory;
	std::filesystem::path ResourceDirectory;
	std::filesystem::path ScriptDirectory;
	std::filesystem::path ResourceMetadataDirectory;

	std::filesystem::path SceneDirectory;
	std::filesystem::path TextureDirectory;
	std::filesystem::path MeshDirectory;
	std::filesystem::path PrefabDirectory;
	std::filesystem::path ShaderDirectory;

	u32 StartScene{ 0 };
};

struct Project
{
	ProjectProperties Properties{};
	std::filesystem::path ProjectDirectory;
	std::filesystem::path ProjectFilePath;
	std::filesystem::path AssetRegistryFilePath;
};

void CreateNewProject();
void LoadProject(std::filesystem::path projectPath);
void UnloadProject();
void SaveProject();
const Project& GetActiveProject();

void RefreshAllAssets();

const std::filesystem::path& GetProjectDirectory();
const std::filesystem::path& GetAssetRegistryPath();
const std::filesystem::path& GetAssetDirectory();
const std::filesystem::path& GetResourceDirectory();
const std::filesystem::path& GetSceneDirectory();
const std::filesystem::path& GetScriptDirectory();
const std::filesystem::path& GetTextureDirectory();
const std::filesystem::path& GetMeshDirectory();
const std::filesystem::path& GetPrefabDirectory();
const std::filesystem::path& GetShaderDirectory();

}