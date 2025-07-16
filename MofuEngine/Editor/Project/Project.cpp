#include "Project.h"
#include <fstream>

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
			out << YAML::Key << "AssetDirectory" << YAML::Value << properties.AssetDirectory.string();
			out << YAML::Key << "ScriptDirectory" << YAML::Value << properties.ScriptDirectory.string();
			out << YAML::Key << "ResourceDirectory" << YAML::Value << properties.ResourceDirectory.string();
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
	properties.AssetDirectory = projectNode["AssetDirectory"].as<std::string>();
	properties.ScriptDirectory = projectNode["ScriptDirectory"].as<std::string>();
	properties.ResourceDirectory = projectNode["ResourceDirectory"].as<std::string>();
	properties.StartScene = projectNode["StartScene"].as<u32>();

	return true;
}

void 
UnloadProject()
{

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
}

void 
LoadProject(std::filesystem::path projectPath)
{
	UnloadProject();
	activeProject = {};

	if (DeserializeProject(projectPath))
	{
		activeProject.ProjectFilePath = projectPath;
		activeProject.ProjectDirectory = projectPath.parent_path();
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

const std::filesystem::path 
GetAssetPath()
{
	return activeProject.ProjectDirectory / activeProject.Properties.AssetDirectory;
}

const std::filesystem::path
GetResourcePath()
{
	return activeProject.ProjectDirectory / activeProject.Properties.ResourceDirectory;
}

}