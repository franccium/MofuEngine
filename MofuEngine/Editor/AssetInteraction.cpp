#include "AssetInteraction.h"
#include "ECS/Entity.h"
#include "Graphics/GeometryData.h"
#include "Graphics/Renderer.h"
#if RAYTRACING
#include "Graphics/D3D12/D3D12Content/D3D12Geometry.h"
#endif
#include "Content/ContentManagement.h"
#include "Content/ResourceCreation.h"
#include "Content/ShaderCompilation.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/ComponentRegistry.h"
#include "SceneEditorView.h"
#include "Editor/Project/Project.h"
#include "Content/EditorContentManager.h"
#include "Material.h"
#include <stack>
#include "Physics/BodyManager.h"
#include "Physics/PhysicsShapes.h"
#include "Physics/PhysicsCore.h"

namespace mofu::editor::assets {
namespace {
void
SerializeEntityHierarchy(YAML::Emitter& out, const Vec<ecs::Entity>& entities)
{
	ecs::Entity parent{ entities.front() };

	////FIXME: test
	//if (ecs::scene::HasComponent<ecs::component::Collider>(parent))
	//{
	//	TODO_("");
	//	ecs::component::Collider& collider{ ecs::scene::GetEntityComponent<ecs::component::Collider>(parent) };
	//	auto current{ content::assets::GetAsset(collider.ShapeAsset) };
	//	const JPH::BodyID bodyId{ collider.BodyID };
	//	JPH::BodyLockRead lock{ mofu::physics::core::PhysicsSystem().GetBodyLockInterface(), bodyId };
	//	const JPH::Shape* shape{ lock.GetBody().GetShape() };
	//	content::AssetHandle a{ physics::shapes::SaveShape(shape, current->ImportedFilePath) };
	//	collider.ShapeAsset = a;
	//}

	assert(ecs::scene::HasComponent<ecs::component::Parent>(parent));
	//TODO: can use sth better for sure
	std::stack<std::pair<id_t, u32>> lastParent{}; // pairs parent id to his index for deserialization
	lastParent.push({ parent, 0 });
	Vec<u32> parents(entities.size());
	u32 currentIndex{};

	assert(out.good()); // TODO: the hell does that mean
	// TODO: prefab handle system if needed
	out << YAML::BeginMap;
	out << YAML::Key << "Prefab" << YAML::Value << Guid{};

	out << YAML::Key << "Entities" << YAML::Value << YAML::BeginMap;

	for (ecs::Entity entity : entities)
	{
		const ecs::EntityData& entityData{ ecs::scene::GetEntityData(entity) };
		const ecs::EntityBlock* const block{ ecs::scene::GetEntityData(entity).block };

		out << YAML::Key << entity;
		out << YAML::Value << YAML::BeginMap;

		out << YAML::Key << "Component IDs" << YAML::BeginSeq;
		for (auto c : block->GetComponentView())
		{
			out << c;
		}
		out << YAML::EndSeq;

		if (block->Signature.test(ecs::component::ID<ecs::component::Parent>))
		{
			parents[currentIndex] = lastParent.top().second;
			lastParent.push({ entity, currentIndex });
		}
		else
		{
			if (lastParent.top().first != ecs::scene::GetComponent<ecs::component::Child>(entity).ParentEntity)
			{
				lastParent.pop();
			}
			parents[currentIndex] = lastParent.top().second;
		}

		{
			out << YAML::Key << "Components";
			out << YAML::Value << YAML::BeginMap;

			ecs::ForEachComponent(block, entityData.row, [&out](ecs::ComponentID cid, u8* data) {
				out << YAML::Key << ecs::component::ComponentNames[cid];
				ecs::component::SerializeLUT[cid](out, data);
				});
			out << YAML::EndMap;
		} // Components

		out << YAML::EndMap;
		currentIndex++;
	} // Entity

	out << YAML::EndMap;

	{
		out << YAML::Key << "Parents" << YAML::Value << YAML::BeginSeq;
		for (auto p : parents) out << p;
		out << YAML::EndSeq;
	} // Parents

	out << YAML::EndMap;
}

void
DeserializeEntityHierarchy(const YAML::Node& entityHierarchyData, Vec<ecs::Entity>& entities)
{
	using namespace ecs;

	//TODO: think of something better?
	struct RenderableEntitySpawnContext
	{
		ecs::Entity	entity;
		ecs::component::RenderMesh& Mesh;
		ecs::component::RenderMaterial& Material;
#if RAYTRACING
		ecs::component::PathTraceable& PathTraceable;
#endif
		bool hasCollider;
		ecs::component::Collider& Collider;
	};
	Vec<RenderableEntitySpawnContext> renderables{};

	// TODO: prefab handle system if needed
	Guid prefabID{ entityHierarchyData["Prefab"].as<u64>() };

	auto entityNodes{ entityHierarchyData["Entities"] };
	for (auto entityNode : entityNodes)
	{
		// get the Cet mask, find a block and initialize space for the components
		// then go over all component IDs and fill their data
		const YAML::Node& componentInfo = entityNode.second;
		const YAML::Node& componentIDs{ componentInfo["Component IDs"] };
		Vec<ComponentID> cids;
		CetMask mask{};
		for (auto a : componentIDs)
		{
			ComponentID cid{ a.as<ComponentID>() };
			cids.emplace_back(cid);
			mask.set((size_t)cid, 1);
		}
#if RAYTRACING
		mask.set(component::ID<component::PathTraceable>, 1);
#endif

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
			component::Collider col{};
#if RAYTRACING
			component::PathTraceable& pt{ ecs::scene::GetComponent<component::PathTraceable>(entity) };
			renderables.emplace_back(entity, mesh, material, pt, false, col); //FIXME: doesn't work with submeshes
#else
			renderables.emplace_back(entity, mesh, material, false, col); //FIXME: doesn't work with submeshes
#endif
		}

		if (mask.test(component::ID<component::Collider>))
		{
			component::Collider& collider{ ecs::scene::GetComponent<component::Collider>(entity) };
			renderables.back().Collider = collider;
			renderables.back().hasCollider = true;
		}
	} // Entities

	auto parentNodes{ entityHierarchyData["Parents"] };
	{
		//TODO: noly works if its the only thing adding entities at the moment and if all the entities are from the same generation
		Entity firstParentID{ entities[0] };
		//NOTE: assumes the first entity is never a child
		for (u32 i{ 1 }; i < entities.size(); ++i)
		{
			Entity parentID{ id::Index(firstParentID) + parentNodes[i].as<u32>() };
			ecs::scene::GetComponent<component::Child>(entities[i]).ParentEntity = parentID;
		}
	} // Parents

	if (!renderables.empty())
	{
		const content::AssetHandle parentGeometryHandle{ renderables[0].Mesh.MeshAsset };
		id_t geometry{ content::assets::CreateResourceFromHandle(parentGeometryHandle) };
		const content::UploadedGeometryInfo uploadedGeometryInfo{ content::GetLastUploadedGeometryInfo() };
		assert(uploadedGeometryInfo.SubmeshCount == renderables.size()); //TODO: for now thats true
		u32 i{ 0 };
		for (auto& e : renderables)
		{
			e.Mesh.MeshID = uploadedGeometryInfo.SubmeshGpuIDs[i++];
			
			if (content::IsValid(e.Material.MaterialAsset))
			{
				e.Material.MaterialID = content::assets::GetResourceFromAsset(e.Material.MaterialAsset, content::AssetType::Material, false);
				if (!id::IsValid(e.Material.MaterialID))
				{
					// load a new material
					graphics::MaterialInitInfo mat{};
					material::LoadMaterialDataFromAsset(mat, e.Material.MaterialAsset);
					e.Material.MaterialID = content::CreateMaterial(mat, e.Material.MaterialAsset);
				}
			}
			else
			{
				e.Material.MaterialID = content::GetDefaultMaterial();
				e.Material.MaterialAsset = content::assets::GetAssetFromResource(e.Material.MaterialID, content::AssetType::Material);
			}
			e.Mesh.RenderItemID = graphics::AddRenderItem(e.entity, e.Mesh.MeshID, e.Material.MaterialCount, e.Material.MaterialID);
#if RAYTRACING
			e.PathTraceable.MeshInfo = graphics::d3d12::content::geometry::GetMeshInfo(e.Mesh.MeshID);
#endif

			if (e.hasCollider)
			{
				assert(content::IsValid(e.Collider.ShapeAsset));
				JPH::Ref<JPH::Shape> physicsShape{ physics::shapes::LoadShape(e.Collider.ShapeAsset) };
				if(ecs::scene::HasComponent<ecs::component::StaticObject>(e.entity))
					physics::AddStaticBody(physicsShape, e.entity);
				else
					physics::AddDynamicBody(physicsShape, e.entity);
			}
		}
		// TODO: needed for materials, force this somewhere
		content::assets::PairAssetWithResource(parentGeometryHandle, uploadedGeometryInfo.GeometryContentID, content::AssetType::Mesh);
	}
}
} // anonymous namespace

void
DropModelIntoScene(std::filesystem::path modelPath, u32* materials /* = nullptr */)
{
	id_t assetId{ content::CreateResourceFromAsset(modelPath, content::AssetType::Mesh) }; //FIXME: this assumes 1 LOD
	content::UploadedGeometryInfo uploadedGeometryInfo{ content::GetLastUploadedGeometryInfo() };
	u32 submeshCount{ uploadedGeometryInfo.SubmeshCount };

	bool hasMaterials{ materials != nullptr };
	const id_t defaultMat{ content::GetDefaultMaterial() };

	/*graphics::MaterialInitInfo materialInfo{};
	content::AssetHandle matHandle{content::}
	material::LoadMaterialDataFromAsset(materialInfo, )*/


	v3 pos{ 0.f, 0.f, 20.f };
	quat rot{ quatIndentity };
	v3 scale{ 1.f, 1.f, 1.f };
	ecs::component::LocalTransform lt{ {}, pos, rot, scale };
	ecs::component::WorldTransform wt{};
	ecs::component::RenderMaterial material{};
	ecs::component::RenderMesh mesh{};
	ecs::component::NameComponent name{};
#if RAYTRACING
	ecs::component::PathTraceable pt{};
#endif

	// root
	mesh.MeshID = uploadedGeometryInfo.GeometryContentID;
	material.MaterialCount = 1;
	material.MaterialID = hasMaterials ? materials[0] : defaultMat;
	material.MaterialAsset = content::assets::GetAssetFromResource(material.MaterialID, content::AssetType::Material);

	struct RenderableEntitySpawnContext
	{
		ecs::Entity	entity;
		ecs::component::RenderMesh Mesh;
		ecs::component::RenderMaterial Material;
		bool isChild{ true };
		ecs::component::Child child;
#if RAYTRACING
		ecs::component::PathTraceable pt{};
#endif
	};
	Vec<RenderableEntitySpawnContext> spawnedEntities(submeshCount);
	std::string filename{ modelPath.stem().string() };
	strcpy(name.Name, filename.data());

	// create root entity
	ecs::component::Parent parentEntity{ {} };
#if RAYTRACING
	pt.MeshInfo = graphics::d3d12::content::geometry::GetMeshInfo(mesh.MeshID);
	ecs::EntityData& rootEntityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform,
		ecs::component::RenderMesh, ecs::component::RenderMaterial, ecs::component::Parent, ecs::component::NameComponent,
	ecs::component::PathTraceable>(
			lt, wt, mesh, material, parentEntity, name, pt) };
	spawnedEntities[0] = { rootEntityData.id, mesh, material, false, pt};
#else
	ecs::EntityData& rootEntityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform,
		ecs::component::RenderMesh, ecs::component::RenderMaterial, ecs::component::Parent, ecs::component::NameComponent>(
			lt, wt, mesh, material, parentEntity, name) };
	spawnedEntities[0] = { rootEntityData.id, mesh, material, false };
#endif

	ecs::component::Child child{ {}, rootEntityData.id };
	pos = {};
	rot = quatIndentity;
	lt.Position = pos;
	lt.Rotation = rot;
	for (u32 i{ 1 }; i < submeshCount; ++i)
	{
		id_t meshId{ uploadedGeometryInfo.SubmeshGpuIDs[i] };
		mesh.MeshID = meshId;
		material.MaterialID = hasMaterials ? materials[i] : defaultMat;
		material.MaterialCount = 1;
		material.MaterialAsset = content::assets::GetAssetFromResource(material.MaterialID, content::AssetType::Material);
		snprintf(name.Name, ecs::component::NAME_LENGTH, "child %u", i);

#if RAYTRACING
		pt.MeshInfo = graphics::d3d12::content::geometry::GetMeshInfo(mesh.MeshID);
		ecs::EntityData& e{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform,
			ecs::component::RenderMesh, ecs::component::RenderMaterial, ecs::component::Child, ecs::component::NameComponent,
		ecs::component::PathTraceable>(
				lt, wt, mesh, material, child, name, pt) };
		assert(ecs::scene::GetComponent<ecs::component::Child>(e.id).ParentEntity == child.ParentEntity);
		spawnedEntities[i] = { e.id, mesh, material, true, child, pt };
#else
		ecs::EntityData& e{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform,
			ecs::component::RenderMesh, ecs::component::RenderMaterial, ecs::component::Child, ecs::component::NameComponent>(
				lt, wt, mesh, material, child, name) };
		assert(ecs::scene::GetComponent<ecs::component::Child>(e.id).ParentEntity == child.ParentEntity);
		spawnedEntities[i] = { e.id, mesh, material, true, child };
#endif
	}

	for (auto& c : spawnedEntities)
	{
		if (c.isChild)
		{
			assert(c.child.ParentEntity == child.ParentEntity);
			assert(ecs::scene::GetComponent<ecs::component::Child>(c.entity).ParentEntity == child.ParentEntity);
		}
		ecs::component::RenderMesh& mesh{ ecs::scene::GetComponent<ecs::component::RenderMesh>(c.entity) };
		mesh.RenderItemID = graphics::AddRenderItem(c.entity, c.Mesh.MeshID, c.Material.MaterialCount, c.Material.MaterialID);
		editor::AddEntityToSceneView(c.entity);
	}
}

void
AddFBXImportedModelToScene(const content::FBXImportState& state, bool extractMaterials)
{
	Prefab prefab{};
	prefab.InitializeFromFBXState(state, extractMaterials);
	if (extractMaterials)
	{
		prefab.ExtractMaterials();
		content::assets::SaveAssetRegistry();
		prefab.Instantiate(mofu::ecs::scene::GetCurrentScene());
	}
	else
	{
		prefab.Instantiate(mofu::ecs::scene::GetCurrentScene());
	}
}

void
SerializeEntityHierarchy(const Vec<ecs::Entity>& entities)
{
	using namespace ecs;

	Entity parent{ entities.front() };
	const char* parentName{ scene::GetComponent<component::NameComponent>(parent).Name };
	std::string prefabFilename{ parentName };
	prefabFilename += content::PREFAB_FILE_EXTENSION;
	const std::filesystem::path resourcesPath{ mofu::editor::project::GetResourceDirectory() };
	std::filesystem::path prefabPath{ resourcesPath / "Prefabs" / prefabFilename };
	
	YAML::Emitter out;

	SerializeEntityHierarchy(out, entities);

	std::ofstream outFile(prefabPath);
	outFile << out.c_str();
}

void
SerializeScene(const ecs::scene::Scene& scene, const Vec<Vec<ecs::Entity>>& hierarchies)
{
	//TODO: maybe it makes sense to serialize by blocks for performance

	/*for(auto [entity, parent]
		: ecs::scene::GetRW<ecs::component::Parent>())
	{
		SerializeEntityHierarchy()
	}*/
	constexpr u32 maxSceneNameLength{ ecs::scene::Scene::MAX_NAME_LENGTH + 4 };
	char sceneFilename[maxSceneNameLength];
	snprintf(sceneFilename, maxSceneNameLength, "%s.sc", scene.Name);
	std::filesystem::path scenePath{ mofu::editor::project::GetSceneDirectory() / sceneFilename };

	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Scene" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "Scene ID" << YAML::Value << scene.GetSceneID();

	out << YAML::Key << "Hierarchies" << YAML::Value << YAML::BeginMap;
	u32 idx{ 0 };
	for (const Vec<ecs::Entity>& hierarchy : hierarchies)
	{
		out << YAML::Key << idx << YAML::Value;
		SerializeEntityHierarchy(out, hierarchy);
		idx++;
	}

	out << YAML::EndMap;

	out << YAML::EndMap;

	std::ofstream outFile(scenePath);
	outFile << out.c_str();
}

void
LoadScene(Vec<Vec<ecs::Entity>>& hierarchies, const std::filesystem::path& path)
{
	ecs::scene::UnloadScene();
	std::string sceneName{ path.stem().string() };
	ecs::scene::CreateScene(sceneName.c_str());

	YAML::Node data = YAML::LoadFile(path.string());

	const auto& sceneData{ data["Scene"] };
	u32 sceneID{ sceneData["Scene ID"].as<u32>() };
	const auto& hierarchiesData{ sceneData["Hierarchies"] };

	u32 hierarchyCount{ (u32)hierarchiesData.size() };
	u32 hierarchyIdx{ 0 };
	for (const auto& hierarchyNode : hierarchiesData)
	{
		hierarchies.emplace_back();
		const YAML::Node& entityHierarchyData{ hierarchyNode.second };

		DeserializeEntityHierarchy(entityHierarchyData, hierarchies[hierarchyIdx]);
		hierarchyIdx++;
	}
}


void
DeserializeEntityHierarchy(Vec<ecs::Entity>& entities, const std::filesystem::path& path)
{
	YAML::Node data = YAML::LoadFile(path.string());

	DeserializeEntityHierarchy(data, entities);
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
		if (content::IsValid(_materialAssets[i]))
		{
			//materialIDs[i] = content::CreateMaterial(_materialInfos[i]);
			id_t matId{ content::assets::GetResourceFromAsset(_materialAssets[i], content::AssetType::Material, false) };
			if (id::IsValid(matId))
			{
				// if the material is already loaded, reuse it
				materialIDs[i] = matId;
			}
			else
			{
				// load a new material
				graphics::MaterialInitInfo mat{};
				material::LoadMaterialDataFromAsset(mat, _materialAssets[i]);
				materialIDs[i] = content::CreateMaterial(mat, _materialAssets[i]);
			}
		}
		else
		{
			materialIDs[i] = content::GetDefaultMaterial();
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
	
	mesh.MeshID = uploadedGeometryInfo.GeometryContentID;
	mesh.MeshAsset = _meshAssets[0];
	material.MaterialCount = 1;
	material.MaterialID = materialIDs[0];
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

	snprintf(name.Name, ecs::component::NAME_LENGTH, "%s", _names[0].c_str());
	name.Name[15] = '\0';
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
		material.MaterialID = materialIDs[i];
		material.MaterialCount = 1;
		material.MaterialAsset = _materialAssets[i];

		snprintf(name.Name, ecs::component::NAME_LENGTH, "%s", i < _names.size() ? _names[i].c_str() : _names[0].c_str());

		ecs::EntityData& e{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform,
			ecs::component::RenderMesh, ecs::component::RenderMaterial, ecs::component::Child, ecs::component::NameComponent>(
				lt, wt, mesh, material, child, name) };
		assert(ecs::scene::GetComponent<ecs::component::Child>(e.id).ParentEntity == child.ParentEntity);
		spawnedEntities[i] = { e.id, mesh, material, true, child };
	}

	u32 entityIdx{ 0 };
	for (auto& c : spawnedEntities)
	{
		if (c.isChild)
		{
			assert(c.child.ParentEntity == child.ParentEntity);
			assert(ecs::scene::GetComponent<ecs::component::Child>(c.entity).ParentEntity == child.ParentEntity);
		}


		ecs::component::RenderMesh& mesh{ ecs::scene::GetComponent<ecs::component::RenderMesh>(c.entity) };
		mesh.RenderItemID = graphics::AddRenderItem(c.entity, c.Mesh.MeshID, c.Material.MaterialCount, c.Material.MaterialID);
		editor::AddEntityToSceneView(c.entity);
		//FIXME: has to be there cause i add entity to transform hierarchy in AddEntityToSceneView() which makes no sense; cant migrate the entity because of that; think of creating some buffer for the new entity before adding it
		if (!_joltMeshShapes.empty() && _joltMeshShapes[entityIdx].GetPtr() != nullptr)
		{
			if(_isStaticBody)
				physics::AddStaticBody(_joltMeshShapes[entityIdx], c.entity);
			else
				physics::AddDynamicBody(_joltMeshShapes[entityIdx], c.entity);
			ecs::component::Collider& col{ ecs::scene::GetComponent<ecs::component::Collider>(c.entity) };
			col.ShapeAsset = _joltShapeAssets[entityIdx];
		}
		entityIdx++;
	}

	// TODO: needed for materials, force this somewhere
	content::assets::PairAssetWithResource(_meshAssets[0], uploadedGeometryInfo.GeometryContentID, content::AssetType::Mesh);

	const u32 lightSet{ graphics::light::GetCurrentLightSetKey() };
	for(const content::LightInitInfo& lightInfo : _lights)
	{
		ecs::component::Light light{};
		ecs::component::WorldTransform wt{};
		ecs::component::NameComponent name{};
		assert(!spawnedEntities.empty());
		ecs::component::Child child{ {}, spawnedEntities[0].entity };
		snprintf(name.Name, ecs::component::NAME_LENGTH, "%s", lightInfo.Name.c_str());
		ecs::Entity entity{};

		switch (lightInfo.Type)
		{
			case graphics::light::LightType::Point:
			{
				ecs::component::PointLight pointLight{};
				pointLight.Range = lightInfo.Range;
				pointLight.Color = lightInfo.Color;
				pointLight.Intensity = lightInfo.Intensity;
				entity = ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform, ecs::component::Light, ecs::component::PointLight, ecs::component::CullableLight, ecs::component::NameComponent, ecs::component::Child>(lightInfo.Transform, wt, light, pointLight, {}, name, child).id;
				break;
			}
			case graphics::light::LightType::Spot:
			{
				ecs::component::SpotLight spotLight{};
				spotLight.Range = lightInfo.Range;
				spotLight.Color = lightInfo.Color;
				spotLight.Intensity = lightInfo.Intensity;
				spotLight.Umbra = lightInfo.InnerConeAngle;
				spotLight.Penumbra = lightInfo.OuterConeAngle;
				entity = ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform, ecs::component::Light, ecs::component::SpotLight, ecs::component::CullableLight, ecs::component::NameComponent, ecs::component::Child>(lightInfo.Transform, wt, light, spotLight, {}, name, child).id;
				break;
			}
			case graphics::light::LightType::Directional:
			{
				ecs::component::DirectionalLight dirLight{};
				dirLight.Color = lightInfo.Color;
				dirLight.Intensity = lightInfo.Intensity;
				//dirLight.Direction
				entity = ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform, ecs::component::Light, ecs::component::DirectionalLight, ecs::component::NameComponent, ecs::component::Child>(lightInfo.Transform, wt, light, dirLight, name, child).id;
				break;
			}
		}

		editor::AddEntityToSceneView(entity);
		graphics::light::AddLightToLightSet(lightSet, entity, lightInfo.Type);
	}
}

void
Prefab::InitializeFromFBXState(const content::FBXImportState& state, bool extractMaterials)
{
	_resourceBasePath = state.ModelResourcePath;
	_isStaticBody = state.ImportSettings.IsStatic;
	_name = state.OutModelFile.stem().string();
	_geometryPath = state.OutModelFile;
	_textureImageFiles = state.ImageFiles;
	assert(!state.MeshNames.empty());
	_names = state.MeshNames;

	_meshAssets.emplace_back(content::assets::GetHandleFromImportedPath(state.OutModelFile));

	u32 meshCount{ (u32)state.LodGroups[0].Meshes.size() };

	_materialInfos.resize(meshCount);
	_materials.resize(meshCount);

	_joltMeshShapes = std::move(state.JoltMeshShapes);
	_lights = std::move(state.Lights);

	if (extractMaterials)
	{
		for (u32 meshIdx{ 0 }; meshIdx < meshCount; ++meshIdx)
		{
			content::Mesh mesh{ state.LodGroups[0].Meshes[meshIdx] };
			graphics::MaterialInitInfo& info{ _materialInfos[meshIdx] };

			const id_t materialIdx{ mesh.MaterialIndices[0] };
			bool hasValidMaterial{ id::IsValid(materialIdx) };
			if (hasValidMaterial)
			{
				_materials[meshIdx] = state.Materials[materialIdx];
			}
			else
			{
				// default material
				_materials[meshIdx] = material::GetDefaultEditorMaterial();
			}
			log::Info("Mesh Index: %u; Material Index: %u ", meshIdx, materialIdx);

			if (hasValidMaterial)
			{
				material::EditorMaterial& mat{ _materials[meshIdx] };

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
				std::pair<id_t, id_t> vsps{ content::GetDefaultPsVsShadersTextured() };
				info.ShaderIDs[shaders::ShaderType::Vertex] = vsps.first;
				info.ShaderIDs[shaders::ShaderType::Pixel] = vsps.second;
				mat.ShaderIDs[shaders::ShaderType::Vertex] = vsps.first;
				mat.ShaderIDs[shaders::ShaderType::Pixel] = vsps.second;
			}
		}
	}
	else
	{
		_materialAssets.resize(meshCount);
		for (u32 meshIdx{ 0 }; meshIdx < meshCount; ++meshIdx)
		{
			_materialAssets[meshIdx] = content::INVALID_HANDLE;
			//material::EditorMaterial& mat{ _materials[meshIdx] };

			//mat = material::GetDefaultEditorMaterialUntextured();
		}
	}

	if (!_joltMeshShapes.empty())
	{
		std::filesystem::path shapesBasePath{ state.ModelResourcePath / "Shapes" };
		if (!std::filesystem::exists(shapesBasePath)) std::filesystem::create_directory(shapesBasePath);
		_joltShapeAssets.resize(_joltMeshShapes.size());
		for (u32 i{ 0 }; i < _joltMeshShapes.size(); ++i)
		{
			if (!_joltMeshShapes[i])
			{
				_joltShapeAssets[i] = content::INVALID_HANDLE;
				continue;
			}
			std::filesystem::path savePath{ shapesBasePath };
			char filename[32]{};
			snprintf(filename, 32, "col%u.ps", i);
			savePath.append(filename);
			_joltShapeAssets[i] = physics::shapes::SaveShape(_joltMeshShapes[i], savePath);
		}
	}
}

void
Prefab::ExtractMaterials()
{
	std::filesystem::path materialBasePath{ _resourceBasePath / "Materials" };
	if (!std::filesystem::exists(materialBasePath)) std::filesystem::create_directory(materialBasePath);

	std::string meshNameTrimmed{ _name.substr(0, 24) };

	u32 materialCount{ (u32)_materials.size() };
	for (u32 matIdx{ 0 }; matIdx < materialCount; ++matIdx)
	{
		material::EditorMaterial& mat{ _materials[matIdx] };
		graphics::MaterialInitInfo& info{ _materialInfos[matIdx] };

		std::filesystem::path matPath{ materialBasePath };
		char filename[32];
		snprintf(filename, 32, "%s_%u.mat", meshNameTrimmed.c_str(), matIdx);
		matPath.append(filename);
		material::PackMaterialAsset(mat, matPath);
		content::AssetHandle handle{ material::CreateMaterialAsset(matPath) };
		log::Info("Extracted material: %s", filename);

		assert(handle != content::INVALID_HANDLE);
		_materialAssets.emplace_back(handle);
	}
}

}