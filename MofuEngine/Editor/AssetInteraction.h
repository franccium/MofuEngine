#pragma once
#include "CommonHeaders.h"
#include <filesystem>
#include "Content/AssetImporter.h"
#include "ECS/Scene.h"
#include "Material.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>

namespace mofu::editor::assets
{
class Prefab
{
public:

	void Instantiate(const ecs::scene::Scene& scene);
	void InitializeFromFBXState(const content::FBXImportState& state, bool extractMaterials);
	void ExtractMaterials();

private:
	bool _isStaticBody;
	std::string _name;
	std::filesystem::path _geometryPath;
	Vec<material::EditorMaterial> _materials;
	Vec<graphics::MaterialInitInfo> _materialInfos;
	Vec<std::string> _textureImageFiles;
	Vec<std::string> _names;

	Vec<content::AssetHandle> _meshAssets;
	Vec<content::AssetHandle> _materialAssets;

	Vec<JPH::Ref<JPH::Shape>> _joltMeshShapes;
	Vec<content::AssetHandle> _joltShapeAssets;
};

void DropModelIntoScene(std::filesystem::path modelPath, u32* materials = nullptr);
void AddFBXImportedModelToScene(const content::FBXImportState& state, bool extractMaterials);

void SerializeEntityHierarchy(const Vec<ecs::Entity>& entities);
void DeserializeEntityHierarchy(Vec<ecs::Entity>& entities, const std::filesystem::path& path);
void SerializeScene(const ecs::scene::Scene& scene, const Vec<Vec<ecs::Entity>>& hierarchies);
void LoadScene(Vec<Vec<ecs::Entity>>& hierarchies, const std::filesystem::path& path);
}