#include "SceneSerializer.h"
#include "fstream"

namespace mofu::ecs::scene {
namespace {
void SerializeEntity(YAML::Emitter& out)
{
	out << YAML::BeginMap;

	// ID

	// Components

	out << YAML::EndMap;
}

}

SceneSerializer::SceneSerializer(Scene& scene) : _scene{ scene }
{

}

void SceneSerializer::SerializeForEditor(const char* filePath) const
{
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Scene" << YAML::Value << "Scene Name";

	//TODO: foreach entity SerializeEntity(out);
	
	out << YAML::EndMap;

	std::ofstream outFile{ filePath };
	outFile << out.c_str();
}

void SceneSerializer::DeserializeForEditor(const char* filePath) const
{
}


}