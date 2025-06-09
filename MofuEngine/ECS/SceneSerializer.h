#pragma once
#include "CommonHeaders.h"
#include "yaml-cpp/yaml.h"
#include "Scene.h"

namespace mofu::ecs::scene {

class SceneSerializer {
public:
	SceneSerializer(Scene& scene);

	void SerializeForEditor(const char* filePath) const;
	void DeserializeForEditor(const char* filePath) const;

private:
	Scene& _scene;
};

}