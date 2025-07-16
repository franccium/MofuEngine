#pragma once
#include "CommonHeaders.h"
#include <filesystem>
#include "Content/AssetImporter.h"
#include "ECS/Scene.h"

namespace mofu::editor::assets
{
class Prefab
{
public:

	void Instantiate(const ecs::scene::Scene& scene);
	void InitializeFromFBXState(const content::FBXImportState& state);
	void InitializeFromHierarchy(const Vec<ecs::Entity> entities);

private:
	std::string _name;
	std::string _geometryPath;
	Vec<graphics::MaterialInitInfo> _materialInfos;
	Vec<std::string> _textureImageFiles;
};

void DropModelIntoScene(std::filesystem::path modelPath, u32* materials = nullptr);
void AddFBXImportedModelToScene(const content::FBXImportState& state);
}