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

namespace mofu::editor {
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
DropModelIntoScene(std::filesystem::path modelPath)
{
	id_t assetId{ LoadAsset(modelPath, content::AssetType::Mesh) }; //FIXME: this assumes 1 LOD
	content::UploadedGeometryInfo uploadedGeometryInfo{ content::GetLastUploadedGeometryInfo() };
	u32 submeshCount{ uploadedGeometryInfo.SubmeshCount };

	id_t mtlID{ content::GetDefaultMaterial() };
	//u32 materialCount{ 1 };
	u32* materials = new u32[submeshCount];
	for (u32 i{ 0 }; i < submeshCount; ++i)
	{
		materials[i] = mtlID;
	}

	v3 pos{ -3.f, -10.f, 90.f };
	quat rot{ quatIndentity };
	v3 scale{ 1.f, 1.f, 1.f };
	ecs::component::LocalTransform lt{ {}, pos, rot, scale };
	ecs::component::WorldTransform wt{};
	ecs::component::RenderMaterial material{};
	ecs::component::RenderMesh mesh{};

	// root
	mesh.MeshID = uploadedGeometryInfo.GeometryContentID;
	material.MaterialCount = 1;
	material.MaterialIDs = &materials[0];

	struct RenderableEntitySpawnContext
	{
		ecs::Entity	entity;
		ecs::component::RenderMesh Mesh;
		ecs::component::RenderMaterial Material;
	};
	Vec<RenderableEntitySpawnContext> spawnedEntities(submeshCount);

	// create root entity
	ecs::component::Parent parentEntity{ {} };
	ecs::EntityData& rootEntityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform,
		ecs::component::RenderMesh, ecs::component::RenderMaterial, ecs::component::Parent>(lt, wt, mesh, material, parentEntity) };
	spawnedEntities[0] = { rootEntityData.id, mesh, material };

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

		ecs::EntityData& e{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform,
			ecs::component::RenderMesh, ecs::component::RenderMaterial, ecs::component::Child>(lt, wt, mesh, material, child) };
		spawnedEntities[i] = { e.id, mesh, material };
	}

	for (auto& c : spawnedEntities)
	{
		graphics::AddRenderItem(c.entity, c.Mesh.MeshID, c.Material.MaterialCount, c.Material.MaterialIDs);
		editor::AddEntityToSceneView(c.entity);
	}
}

}