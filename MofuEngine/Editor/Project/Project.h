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

const std::filesystem::path GetProjectDirectory();
const std::filesystem::path GetAssetRegistryPath();
const std::filesystem::path GetAssetDirectory();
const std::filesystem::path GetResourceDirectory();

}