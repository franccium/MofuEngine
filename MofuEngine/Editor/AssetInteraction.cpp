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
#include "Content/EditorContentManager.h"
#include "Material.h"

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

	/*graphics::MaterialInitInfo materialInfo{};
	content::AssetHandle matHandle{content::}
	material::LoadMaterialDataFromAsset(materialInfo, )*/


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
		ecs::component::RenderMesh& mesh{ ecs::scene::GetComponent< ecs::component::RenderMesh >(c.entity) };
		mesh.RenderItemID = graphics::AddRenderItem(c.entity, c.Mesh.MeshID, c.Material.MaterialCount, c.Material.MaterialIDs);
		editor::AddEntityToSceneView(c.entity);
	}
}

void 
AddFBXImportedModelToScene(const content::FBXImportState& state)
{
	Prefab prefab{};
	prefab.InitializeFromFBXState(state);
	prefab.ExtractMaterials();
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
	std::filesystem::path prefabPath{ resourcesPath / "Prefabs" / prefabFilename};

	YAML::Emitter out;
	{
		out << YAML::BeginMap;

		for (Entity entity : entities)
		{
			const EntityData& entityData{ scene::GetEntityData(entity) };
			const EntityBlock* const block{ scene::GetEntityData(entity).block };

			out << YAML::Key << entity;
			out << YAML::Value << YAML::BeginMap;

			out << YAML::Key << "Component IDs" << YAML::BeginSeq;
			for (auto c : block->GetComponentView())
			{
				out << c;
			}
			out << YAML::EndSeq;

			{
				out << YAML::Key << "Components";
				out << YAML::Value << YAML::BeginMap;

				ForEachComponent(block, entityData.row, [&out](ComponentID cid, u8* data) {
					out << YAML::Key << component::ComponentNames[cid];
					component::SerializeLUT[cid](out, data);
					});
				out << YAML::EndMap;
			}

			out << YAML::EndMap;
		}

		out << YAML::EndMap;
	}

	std::ofstream outFile(prefabPath);
	outFile << out.c_str();
}

void
DeserializeEntityHierarchy(Vec<ecs::Entity>& entities, const std::filesystem::path& path)
{
	using namespace ecs;

	//TODO: think of something better?
	struct RenderableEntitySpawnContext
	{
		ecs::Entity	entity;
		ecs::component::RenderMesh& Mesh;
		ecs::component::RenderMaterial& Material;
	};
	Vec<RenderableEntitySpawnContext> renderables{};

	YAML::Node data = YAML::LoadFile(path.string());

	for (auto entityNode : data) 
	{
		// get the Cet mask, find a block and initialize space for the components
		// then go over all component IDs and fill their data
		const YAML::Node& componentInfo = entityNode.second;
		const YAML::Node& componentIDs{ componentInfo["Component IDs"]};
		Vec<ComponentID> cids;
		CetMask mask{};
		for (auto a : componentIDs)
		{
			ComponentID cid{ a.as<ComponentID>() };
			cids.emplace_back(cid);
			mask.set((size_t)cid, 1);
		}

		const EntityData& entityData{ ecs::scene::SpawnEntity(mask) };
		const EntityBlock* block{ entityData.block };
		Entity entity{ entityData.id };

		const YAML::Node& components{ componentInfo["Components"] };

		u32 i{ 0 };
		for (auto component : components)
		{
			ComponentID cid{ cids[i++] };
			auto componentData{ component.second };
			u32 offset = block->ComponentOffsets[cid] + component::GetComponentSize(cid) * entityData.row;
			component::DeserializeLUT[cid](componentData, block->ComponentData + offset);
		}

		entities.emplace_back(entity);

		if (mask.test(component::ID<component::RenderMesh>))
		{
			component::RenderMesh& mesh{ ecs::scene::GetComponent<component::RenderMesh>(entity) };
			component::RenderMaterial& material{ ecs::scene::GetComponent<component::RenderMaterial>(entity) };
			renderables.emplace_back(entity, mesh, material);
		}
	}

	if (!renderables.empty())
	{
		content::AssetHandle parentGeometryHandle{ renderables[0].Mesh.MeshAsset };
		id_t geometry{ content::assets::CreateResourceFromHandle(parentGeometryHandle) };
		content::UploadedGeometryInfo uploadedGeometryInfo{ content::GetLastUploadedGeometryInfo() };
		assert(uploadedGeometryInfo.SubmeshCount == renderables.size()); //TODO: for now thats true
		u32 i{ 0 };
		for (auto& e : renderables)
		{
			e.Mesh.MeshID = uploadedGeometryInfo.SubmeshGpuIDs[i++];
			e.Material.MaterialIDs = new id_t[1]; //TODO:
			graphics::MaterialInitInfo mat{};
			material::LoadMaterialDataFromAsset(mat, e.Material.MaterialAsset);
			e.Material.MaterialIDs[0] = content::CreateResourceFromBlob(&mat, content::AssetType::Material);
			e.Mesh.RenderItemID = graphics::AddRenderItem(e.entity, e.Mesh.MeshID, e.Material.MaterialCount, e.Material.MaterialIDs);
		}
	}
}

void 
Prefab::Instantiate([[maybe_unused]] const ecs::scene::Scene& scene)
{
	assert(std::filesystem::exists(_geometryPath));
	//id_t geometryID{ LoadAsset(_geometryPath, content::AssetType::Mesh) };
	assert(!_meshAssets.empty() && !_materialAssets.empty());
	id_t geometryID{ content::assets::CreateResourceFromHandle(_meshAssets[0]) };
	content::UploadedGeometryInfo uploadedGeometryInfo{ content::GetLastUploadedGeometryInfo() };
	u32 submeshCount{ uploadedGeometryInfo.SubmeshCount };

	id_t* materialIDs = new id_t[submeshCount];
	for (u32 i{ 0 }; i < submeshCount; ++i)
	{
		//materialIDs[i] = content::CreateMaterial(_materialInfos[i]);
		graphics::MaterialInitInfo mat{};
		material::LoadMaterialDataFromAsset(mat, _materialAssets[i]);
		materialIDs[i] = content::CreateMaterial(mat);
	}

	v3 pos{ 0.f, 0.f, 0.f };
	quat rot{ quatIndentity };
	v3 scale{ 1.f, 1.f, 1.f };
	ecs::component::LocalTransform lt{ {}, pos, rot, scale };
	ecs::component::WorldTransform wt{};
	ecs::component::RenderMaterial material{};
	ecs::component::RenderMesh mesh{};
	ecs::component::NameComponent name{};
	
	mesh.MeshID = uploadedGeometryInfo.GeometryContentID;
	mesh.MeshAsset = _meshAssets[0];
	material.MaterialCount = 1;
	material.MaterialIDs = &materialIDs[0];
	material.MaterialAsset = _materialAssets[0];

	struct RenderableEntitySpawnContext
	{
		ecs::Entity	entity;
		ecs::component::RenderMesh Mesh;
		ecs::component::RenderMaterial Material;
		bool isChild{ true };
		ecs::component::Child child;
	};
	Vec<RenderableEntitySpawnContext> spawnedEntities(submeshCount);

	std::string filename{ "testPrefab" };
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
		mesh.MeshAsset = _meshAssets[0];
		material.MaterialIDs = &materialIDs[i];
		material.MaterialCount = 1;
		material.MaterialAsset = _materialAssets[1];

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
		ecs::component::RenderMesh& mesh{ ecs::scene::GetComponent< ecs::component::RenderMesh >(c.entity) };
		mesh.RenderItemID = graphics::AddRenderItem(c.entity, c.Mesh.MeshID, c.Material.MaterialCount, c.Material.MaterialIDs);
		editor::AddEntityToSceneView(c.entity);
	}
}

void
Prefab::InitializeFromFBXState(const content::FBXImportState& state)
{
	std::filesystem::path p{ state.OutModelFile };
	_name = p.stem().string();
	_geometryPath = state.OutModelFile;
	_textureImageFiles = state.ImageFiles;

	_meshAssets.emplace_back(content::assets::GetHandleFromImportedPath(state.OutModelFile));

	u32 meshCount{ (u32)state.LodGroups[0].Meshes.size() };

	_materialInfos.resize(meshCount);
	_materials.resize(meshCount);

	for (u32 meshIdx{ 0 }; meshIdx < meshCount; ++meshIdx)
	{
		content::Mesh mesh{ state.LodGroups[0].Meshes[meshIdx] };
		material::EditorMaterial& mat{ _materials[meshIdx] };
		graphics::MaterialInitInfo& info{ _materialInfos[meshIdx] };

		const id_t materialIdx{ mesh.MaterialIndices[0] };
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

		info.TextureCount = material::TextureUsage::Count;
		mat.TextureCount = material::TextureUsage::Count;
		info.TextureIDs = new id_t[material::TextureUsage::Count];

		for (u32 texIdx{ 0 }; texIdx < material::TextureUsage::Count; ++texIdx)
		{
			if (id::IsValid(mat.TextureIDs[texIdx]))
			{
				info.TextureIDs[texIdx] = mat.TextureIDs[texIdx];
			}
			else
			{
				info.TextureIDs[texIdx] = material::GetDefaultTextureID((material::TextureUsage::Usage)texIdx);
				mat.TextureIDs[texIdx] = material::GetDefaultTextureID((material::TextureUsage::Usage)texIdx);
			}
		}

		info.Surface = mat.Surface;
		info.Type = mat.Type;
		bool textured{ true };
		std::pair<id_t, id_t> vsps{ content::GetDefaultPsVsShaders(textured) };
		info.ShaderIDs[graphics::ShaderType::Vertex] = vsps.first;
		info.ShaderIDs[graphics::ShaderType::Pixel] = vsps.second;
		mat.ShaderIDs[graphics::ShaderType::Vertex] = vsps.first;
		mat.ShaderIDs[graphics::ShaderType::Pixel] = vsps.second;
	}
}

void
Prefab::ExtractMaterials()
{
	std::filesystem::path materialBasePath{ project::GetResourceDirectory() / "Materials" };

	u32 materialCount{ (u32)_materials.size() };
	for (u32 matIdx{ 0 }; matIdx < materialCount; ++matIdx)
	{
		material::EditorMaterial& mat{ _materials[matIdx] };
		graphics::MaterialInitInfo& info{ _materialInfos[matIdx] };

		std::filesystem::path matPath{ materialBasePath };
		char filename[32];
		snprintf(filename, 32, "%s%u.mat", _name.c_str(), matIdx);
		matPath.append(filename);
		material::PackMaterialAsset(mat, matPath);
		log::Info("Extracted material: %s", filename);

		content::AssetHandle handle{ content::assets::GetHandleFromImportedPath(matPath) };
		_materialAssets.emplace_back(handle);
	}
}

}