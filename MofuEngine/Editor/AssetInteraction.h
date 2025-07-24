#pragma once
#include "CommonHeaders.h"
#include <filesystem>
#include "Content/AssetImporter.h"
#include "ECS/Scene.h"
#include "Material.h"

namespace mofu::editor::assets
{
class Prefab
{
public:

	void Instantiate(const ecs::scene::Scene& scene);
	void InitializeFromFBXState(const content::FBXImportState& state, bool extractMaterials);
	void ExtractMaterials();

private:
	std::string _name;
	std::filesystem::path _geometryPath;
	Vec<material::EditorMaterial> _materials;
	Vec<graphics::MaterialInitInfo> _materialInfos;
	Vec<std::string> _textureImageFiles;

	Vec<content::AssetHandle> _meshAssets;
	Vec<content::AssetHandle> _materialAssets;
};

void DropModelIntoScene(std::filesystem::path modelPath, u32* materials = nullptr);
void AddFBXImportedModelToScene(const content::FBXImportState& state, bool extractMaterials);

void SerializeEntityHierarchy(const Vec<ecs::Entity>& entities);
void DeserializeEntityHierarchy(Vec<ecs::Entity>& entities, const std::filesystem::path& path);
void SerializeScene(const ecs::scene::Scene& scene, const Vec<Vec<ecs::Entity>>& hierarchies);
void DeserializeScene(Vec<Vec<ecs::Entity>>& hierarchies, const std::filesystem::path& path);
}