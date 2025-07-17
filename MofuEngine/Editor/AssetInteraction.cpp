#include "AssetInteraction.h"
#include "ECS/Entity.h"
#include "Graphics/GeometryData.h"
#include "Graphics/Renderer.h"
#include "Content/ContentManagement.h"
#include "Content/ResourceCreation.h"
#include "Content/ShaderCompilation.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/ComponentRegistry.h"
#include "SceneEditorView.h"
#include "Editor/Project/Project.h"

namespace mofu::editor::assets {
[[nodiscard]] id_t 
LoadAsset(std::filesystem::path path, content::AssetType::type type)
{
	std::unique_ptr<u8[]> buffer;
	u64 size{ 0 };
	assert(std::filesystem::exists(path));
	content::ReadAssetFileNoVersion(std::filesystem::path(path), buffer, size, type);	
	assert(buffer.get());

	id_t assetID{ content::CreateResourceFromBlob(buffer.get(), type) };
	assert(id::IsValid(assetID));
	return assetID;
}

void
DropModelIntoScene(std::filesystem::path modelPath, u32* materials /* = nullptr */)
{
	id_t assetId{ LoadAsset(modelPath, content::AssetType::Mesh) }; //FIXME: this assumes 1 LOD
	content::UploadedGeometryInfo uploadedGeometryInfo{ content::GetLastUploadedGeometryInfo() };
	u32 submeshCount{ uploadedGeometryInfo.SubmeshCount };

	if (!materials)
	{
		id_t mtlID{ content::GetDefaultMaterial() };
		//u32 materialCount{ 1 };
		materials = new u32[submeshCount];
		for (u32 i{ 0 }; i < submeshCount; ++i)
		{
			materials[i] = mtlID;
		}
	}

	v3 pos{ 0.f, 0.f, 0.f };
	quat rot{ quatIndentity };
	v3 scale{ 1.f, 1.f, 1.f };
	ecs::component::LocalTransform lt{ {}, pos, rot, scale };
	ecs::component::WorldTransform wt{};
	ecs::component::RenderMaterial material{};
	ecs::component::RenderMesh mesh{};
	ecs::component::NameComponent name{};

	// root
	mesh.MeshID = uploadedGeometryInfo.GeometryContentID;
	material.MaterialCount = 1;
	material.MaterialIDs = &materials[0];

	struct RenderableEntitySpawnContext
	{
		ecs::Entity	entity;
		ecs::component::RenderMesh Mesh;
		ecs::component::RenderMaterial Material;
		bool isChild{ true };
		ecs::component::Child child;
	};
	Vec<RenderableEntitySpawnContext> spawnedEntities(submeshCount);
	std::string filename{ modelPath.stem().string() };
	strcpy(name.Name, filename.data());

	// create root entity
	ecs::component::Parent parentEntity{ {} };
	ecs::EntityData& rootEntityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform,
		ecs::component::RenderMesh, ecs::component::RenderMaterial, ecs::component::Parent, ecs::component::NameComponent>(
			lt, wt, mesh, material, parentEntity, name) };
	spawnedEntities[0] = { rootEntityData.id, mesh, material, false };

	ecs::component::Child child{ {}, rootEntityData.id };
	pos = {};
	rot = quatIndentity;
	lt.Position = pos;
	lt.Rotation = rot;
	for (u32 i{ 1 }; i < submeshCount; ++i)
	{
		id_t meshId{ uploadedGeometryInfo.SubmeshGpuIDs[i] };
		mesh.MeshID = meshId;
		material.MaterialIDs = &materials[i];
		material.MaterialCount = 1;
		snprintf(name.Name, ecs::component::NAME_LENGTH, "child %u", i);

		ecs::EntityData& e{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform,
			ecs::component::RenderMesh, ecs::component::RenderMaterial, ecs::component::Child, ecs::component::NameComponent>(
				lt, wt, mesh, material, child, name) };
		assert(ecs::scene::GetComponent<ecs::component::Child>(e.id).ParentEntity == child.ParentEntity);
		spawnedEntities[i] = { e.id, mesh, material, true, child };
	}

	for (auto& c : spawnedEntities)
	{
		if (c.isChild)
		{
			assert(c.child.ParentEntity == child.ParentEntity);
			assert(ecs::scene::GetComponent<ecs::component::Child>(c.entity).ParentEntity == child.ParentEntity);
		}
		graphics::AddRenderItem(c.entity, c.Mesh.MeshID, c.Material.MaterialCount, c.Material.MaterialIDs);
		editor::AddEntityToSceneView(c.entity);
	}
}

void 
AddFBXImportedModelToScene(const content::FBXImportState& state)
{
	Prefab prefab{};
	prefab.InitializeFromFBXState(state);
	prefab.Instantiate(mofu::ecs::scene::GetCurrentScene());
}

void 
SerializeEntityHierarchy(const Vec<ecs::Entity>& entities)
{
	using namespace ecs;

	Entity parent{ entities.front() };
	const char* parentName{ scene::GetComponent<component::NameComponent>(parent).Name };
	std::string prefabFilename{ parentName };
	prefabFilename += ".pre";
	const std::filesystem::path resourcesPath{ mofu::editor::project::GetResourceDirectory() };
	std::filesystem::path prefabPath{ resourcesPath / prefabFilename };

	YAML::Emitter out;
	{
		out << YAML::BeginMap;

		for (Entity entity : entities)
		{
			const EntityData& entityData{ scene::GetEntityData(entity) };
			const EntityBlock* const block{ scene::GetEntityData(entity).block };

			const component::LocalTransform& lt{ scene::GetComponent<component::LocalTransform>(entity) };

			out << YAML::Key << entity;
			out << YAML::Value << YAML::BeginMap;

			ForEachComponent(block, entityData.row, [&out](ComponentID cid, u8* data) {
				out << YAML::Key << component::ComponentNames[cid];
				component::SerializeLUT[cid](out, data);
				});

			out << YAML::EndMap;
		}


		out << YAML::EndMap;
	}

	std::ofstream outFile(prefabPath);
	outFile << out.c_str();
}

void 
Prefab::Instantiate([[maybe_unused]] const ecs::scene::Scene& scene)
{
	assert(std::filesystem::exists(_geometryPath));
	id_t geometryID{ LoadAsset(_geometryPath, content::AssetType::Mesh) };
	content::UploadedGeometryInfo uploadedGeometryInfo{ content::GetLastUploadedGeometryInfo() };
	u32 submeshCount{ uploadedGeometryInfo.SubmeshCount };

	id_t* materialIDs = new id_t[submeshCount];
	for (u32 i{ 0 }; i < submeshCount; ++i)
	{
		materialIDs[i] = content::CreateMaterial(_materialInfos[i]);
	}

	v3 pos{ 0.f, 0.f, 0.f };
	quat rot{ quatIndentity };
	v3 scale{ 1.f, 1.f, 1.f };
	ecs::component::LocalTransform lt{ {}, pos, rot, scale };
	ecs::component::WorldTransform wt{};
	ecs::component::RenderMaterial material{};
	ecs::component::RenderMesh mesh{};
	
	mesh.MeshID = uploadedGeometryInfo.GeometryContentID;
	material.MaterialCount = 1;
	material.MaterialIDs = &materialIDs[0];

	struct RenderableEntitySpawnContext
	{
		ecs::Entity	entity;
		ecs::component::RenderMesh Mesh;
		ecs::component::RenderMaterial Material;
		bool isChild{ true };
		ecs::component::Child child;
	};
	Vec<RenderableEntitySpawnContext> spawnedEntities(submeshCount);

	// create root entity
	ecs::component::Parent parentEntity{ {} };
	ecs::EntityData& rootEntityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform,
		ecs::component::RenderMesh, ecs::component::RenderMaterial, ecs::component::Parent>(lt, wt, mesh, material, parentEntity) };
	spawnedEntities[0] = { rootEntityData.id, mesh, material, false };

	ecs::component::Child child{ {}, rootEntityData.id };
	pos = {};
	rot = quatIndentity;
	lt.Position = pos;
	lt.Rotation = rot;
	for (u32 i{ 1 }; i < submeshCount; ++i)
	{
		id_t meshId{ uploadedGeometryInfo.SubmeshGpuIDs[i] };
		mesh.MeshID = meshId;
		material.MaterialIDs = &materialIDs[i];
		material.MaterialCount = 1;

		ecs::EntityData& e{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform,
			ecs::component::RenderMesh, ecs::component::RenderMaterial, ecs::component::Child>(lt, wt, mesh, material, child) };
		assert(ecs::scene::GetComponent<ecs::component::Child>(e.id).ParentEntity == child.ParentEntity);
		spawnedEntities[i] = { e.id, mesh, material, true, child };
	}

	for (auto& c : spawnedEntities)
	{
		if (c.isChild)
		{
			assert(c.child.ParentEntity == child.ParentEntity);
			assert(ecs::scene::GetComponent<ecs::component::Child>(c.entity).ParentEntity == child.ParentEntity);
		}
		graphics::AddRenderItem(c.entity, c.Mesh.MeshID, c.Material.MaterialCount, c.Material.MaterialIDs);
		editor::AddEntityToSceneView(c.entity);
	}
}

void
Prefab::InitializeFromFBXState(const content::FBXImportState& state)
{
	_name = state.OutModelFile;
	_geometryPath = state.OutModelFile;
	_textureImageFiles = state.ImageFiles;

	u32 meshCount{ (u32)state.LodGroups[0].Meshes.size() };

	_materialInfos.resize(meshCount);

	for (u32 meshIdx{ 0 }; meshIdx < meshCount; ++meshIdx)
	{
		content::Mesh mesh{ state.LodGroups[0].Meshes[meshIdx] };
		graphics::MaterialInitInfo& info{ _materialInfos[meshIdx] };
		info.TextureCount = material::TextureUsage::Count;
		info.TextureIDs = new id_t[material::TextureUsage::Count];

		const u32 materialIdx{ mesh.MaterialIndices[0] };
		material::EditorMaterial mat;
		if (id::IsValid(materialIdx))
		{
			mat = state.Materials[materialIdx];
		}
		else
		{
			// default material
			mat = material::GetDefaultEditorMaterial();
		}
		log::Info("Mesh Index: %u; Material Index: %u ", meshIdx, materialIdx);

		for (u32 texIdx{ 0 }; texIdx < material::TextureUsage::Count; ++texIdx)
		{
			info.TextureIDs[texIdx] = id::IsValid(mat.TextureIDs[texIdx])
				? mat.TextureIDs[texIdx]
				: material::GetDefaultTextureID((material::TextureUsage::Usage)texIdx);
		}

		info.Surface = mat.Surface;
		info.Type = mat.Type;
		bool textured{ true };
		std::pair<id_t, id_t> vsps{ content::GetDefaultPsVsShaders(textured) };
		info.ShaderIDs[graphics::ShaderType::Vertex] = vsps.first;
		info.ShaderIDs[graphics::ShaderType::Pixel] = vsps.second;
	}
}

}