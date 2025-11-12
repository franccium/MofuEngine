#include "Project.h"
#include <fstream>
#include "Content/EditorContentManager.h"

namespace mofu::editor::project {
namespace {

Project activeProject;

bool
SerializeProject(const std::filesystem::path& projectPath)
{
	assert(std::filesystem::exists(projectPath) && projectPath.extension() == ".mpj");
	const ProjectProperties& properties{ activeProject.Properties };

	YAML::Emitter out;
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Project" << YAML::Value;
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Name" << YAML::Value << properties.Name;
			out << YAML::Key << "StartScene" << YAML::Value << properties.StartScene;
			out << YAML::EndMap;
		}
		out << YAML::EndMap;
	}

	std::ofstream outFile{ projectPath };
	outFile << out.c_str();

	return true;
}

bool
DeserializeProject(const std::filesystem::path& projectPath)
{
	assert(std::filesystem::exists(projectPath) && projectPath.extension() == ".mpj");
	ProjectProperties& properties{ activeProject.Properties };

	YAML::Node data{ YAML::LoadFile(projectPath.string()) };
	assert(data.IsDefined());

	YAML::Node projectNode{ data["Project"] };
	if (!projectNode) return false;

	properties.Name = projectNode["Name"].as<std::string>();
	properties.StartScene = projectNode["StartScene"].as<u32>();

	return true;
}

void InitializeProjectProperties()
{
	const char* ASSET_DIRECTORY_NAME{ "Assets" };
	const char* SCRIPT_DIRECTORY_NAME{ "Scripts" };
	const char* RESOURCE_DIRECTORY_NAME{ "Resources" };
	const char* RESOURCE_METADATA_DIRECTORY_NAME{ "Metadata" };
	const char* SCENE_DIRECTORY_NAME{ "Scenes" };
	const char* TEXTURE_DIRECTORY_NAME{ "Textures" };
	const char* MESH_DIRECTORY_NAME{ "Meshes" };
	const char* PREFABS_DIRECTORY_NAME{ "Prefabs" };
	const char* SHADERS_DIRECTORY_NAME{ "Shaders" };
	ProjectProperties& properties{ activeProject.Properties };
	properties.AssetDirectory = activeProject.ProjectDirectory / ASSET_DIRECTORY_NAME;
	properties.ResourceDirectory = activeProject.ProjectDirectory / RESOURCE_DIRECTORY_NAME;
	properties.ScriptDirectory = activeProject.ProjectDirectory / SCRIPT_DIRECTORY_NAME;
	properties.SceneDirectory = activeProject.ProjectDirectory / SCENE_DIRECTORY_NAME;
	properties.ResourceMetadataDirectory = properties.ResourceDirectory / RESOURCE_METADATA_DIRECTORY_NAME;
	properties.TextureDirectory = properties.ResourceDirectory / TEXTURE_DIRECTORY_NAME;
	properties.MeshDirectory = properties.ResourceDirectory / MESH_DIRECTORY_NAME;
	properties.PrefabDirectory = properties.ResourceDirectory / PREFABS_DIRECTORY_NAME;
	properties.ShaderDirectory = activeProject.ProjectDirectory / SHADERS_DIRECTORY_NAME;
}

void CreateProjectDirectories(const ProjectProperties& properties)
{
	std::filesystem::create_directory(properties.AssetDirectory);
	std::filesystem::create_directory(properties.ResourceDirectory);
	std::filesystem::create_directory(properties.ScriptDirectory);
	std::filesystem::create_directory(properties.ResourceMetadataDirectory);
	std::filesystem::create_directory(properties.SceneDirectory);
	std::filesystem::create_directory(properties.TextureDirectory);
	std::filesystem::create_directory(properties.MeshDirectory);
	std::filesystem::create_directory(properties.PrefabDirectory);
}

} // anonymous namespace

void
CreateNewProject()
{
	activeProject = {};
	std::string testName = "Test Project";
	activeProject.Properties.Name = testName;
	//TODO: some popups for setup
	activeProject.ProjectFilePath = "Projects/TestProject/TestProject.mpj";
	activeProject.ProjectDirectory = activeProject.ProjectFilePath.parent_path();
	activeProject.AssetRegistryFilePath = activeProject.ProjectDirectory / "AssetRegistry.ar";
	InitializeProjectProperties();
	CreateProjectDirectories(activeProject.Properties);
}

void
UnloadProject()
{
	if (activeProject.ProjectFilePath == std::filesystem::path{}) return;
	content::assets::ShutdownAssetRegistry();
}

void 
LoadProject(std::filesystem::path projectPath)
{
	if (activeProject.ProjectFilePath != std::filesystem::path{}) UnloadProject();
	activeProject = {};

	if (DeserializeProject(projectPath))
	{
		activeProject.ProjectFilePath = projectPath;
		activeProject.ProjectDirectory = projectPath.parent_path();
		activeProject.AssetRegistryFilePath = activeProject.ProjectDirectory / "AssetRegistry.ar";

		InitializeProjectProperties();

		content::assets::InitializeAssetRegistry();
	}
}

void 
SaveProject()
{
	assert(std::filesystem::exists(activeProject.ProjectFilePath));
	
	if (SerializeProject(activeProject.ProjectFilePath))
	{

	}


}

const Project& 
GetActiveProject()
{
	return activeProject;
}

void 
RefreshAllAssets()
{
	//content::assets::ImportAllNotImported();
	content::assets::RegisterAllAssetsOfType(content::AssetType::Material);
}

const std::filesystem::path&
GetProjectDirectory()
{
	return activeProject.ProjectDirectory;
}

const std::filesystem::path&
GetAssetRegistryPath()
{
	return activeProject.AssetRegistryFilePath;
}

const std::filesystem::path&
GetAssetDirectory()
{
	return activeProject.Properties.AssetDirectory;
}

const std::filesystem::path&
GetShaderDirectory()
{
	return activeProject.Properties.ShaderDirectory;
}

const std::filesystem::path&
GetResourceDirectory()
{
	return activeProject.Properties.ResourceDirectory;
}

const std::filesystem::path&
GetSceneDirectory()
{
	return activeProject.Properties.SceneDirectory;
}

const std::filesystem::path&
GetScriptDirectory()
{
	return activeProject.Properties.ScriptDirectory;
}


const std::filesystem::path& 
GetTextureDirectory()
{
	return activeProject.Properties.TextureDirectory;
}

const std::filesystem::path&
GetMeshDirectory()
{
	return activeProject.Properties.MeshDirectory;
}

const std::filesystem::path& 
GetPrefabDirectory()
{
	return activeProject.Properties.PrefabDirectory;
}

}