#include "CommonHeaders.h"
#include <filesystem>
#include "ECS/Entity.h"
#include "Graphics/GeometryData.h"
#include "Graphics/Renderer.h"
#include "Content/ContentManagement.h"
#include "Content/ResourceCreation.h"
#include "Content/ShaderCompilation.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/ComponentRegistry.h"
#include "Editor/AssetInteraction.h"

using namespace mofu;

id_t content::CreateResourceFromBlob(const void* const blob, content::AssetType::type resourceType);

struct MeshTest
{
	id_t MeshID{ id::INVALID_ID };
	ecs::Entity EntityID{ id::INVALID_ID };
};
//MeshTest planeMeshTest{};
Vec<MeshTest> meshTests{};

id_t vsID{ id::INVALID_ID };
id_t psID{ id::INVALID_ID };
id_t mtlID{ id::INVALID_ID };

constexpr const char* TEST_MESH_ASSET_PATH{ "Assets/Generated/plane.geom" };
constexpr const char* TEST_MESH_PATH{ "Assets/Generated/planeModel.model" };
constexpr const char* TEST_IMPORTED_MESH_PATH{ "Assets/Generated/testmodel.model" };
constexpr const char* TEST_BISTRO_MESH_PATH{ "Assets/Generated/BistroInterior.model" };

constexpr u32 OBJECT_COUNT{ 1 };
constexpr u32 MATERIAL_COUNT{ 1 }; //NOTE: ! equal to submesh idCount
constexpr u32 MAX_MATERIALS_PER_MODEL{ 1024 };

u32 loadedModelsCount{ 0 };

Vec<id_t> loadedMeshesIDs{};
Vec<ecs::Entity> loadedEntities{};
Vec<id_t> loadedMaterialIDs{};

[[nodiscard]] id_t 
LoadAsset(const char* path, content::AssetType::type type)
{
	std::unique_ptr<u8[]> buffer;
	u64 size{ 0 };
	assert(path && std::filesystem::exists(path));
	content::ReadAssetFileNoVersion(std::filesystem::path(path), buffer, size, type);	
	assert(buffer.get());

	id_t assetID{ content::CreateResourceFromBlob(buffer.get(), type) };
	assert(id::IsValid(assetID));
	return assetID;
}

[[nodiscard]] id_t
LoadMesh(const char* path)
{
	return LoadAsset(path, content::AssetType::Mesh);
}

void
LoadShaders()
{
	ShaderFileInfo info{};
	info.file = "TestShader.hlsl";
	info.entryPoint = "TestShaderVS";
	info.type = graphics::ShaderType::Vertex;
	const char* shaderPath{ "..\\ExampleApp\\" };

	std::wstring defines[]{L"ELEMENTS_TYPE=1", L"ELEMENTS_TYPE=3"};
	Vec<u32> keys{};
	keys.emplace_back((u32)content::ElementType::StaticNormal);
	keys.emplace_back((u32)content::ElementType::StaticNormalTexture);

	Vec<std::wstring> extraArgs{};
	Vec<std::unique_ptr<u8[]>> vertexShaders{};
	Vec<const u8*> vertexShaderPtrs{};
	for (u32 i{ 0 }; i < _countof(defines); ++i)
	{
		extraArgs.clear();
		extraArgs.emplace_back(L"-D");
		extraArgs.emplace_back(defines[i]);
		vertexShaders.emplace_back(std::move(CompileShader(info, shaderPath, extraArgs)));
		assert(vertexShaders.back().get());
		vertexShaderPtrs.emplace_back(vertexShaders.back().get());
	}

	info.entryPoint = "TestShaderPS";
	info.type = graphics::ShaderType::Pixel;
	Vec<std::unique_ptr<u8[]>> pixelShaders{};

	extraArgs.clear();
	pixelShaders.emplace_back(std::move((CompileShader(info, shaderPath, extraArgs))));
	assert(pixelShaders.back().get());
	const u8* pixelShaderPtrs[]{ pixelShaders[0].get() };

	vsID = content::AddShaderGroup(vertexShaderPtrs.data(), (u32)vertexShaderPtrs.size(), keys.data());
	psID = content::AddShaderGroup(pixelShaderPtrs, 1, &U32_INVALID_ID);
}

void
CreateMaterial()
{
	assert(id::IsValid(vsID) && id::IsValid(psID));

	graphics::MaterialInitInfo info{};
	info.ShaderIDs[graphics::ShaderType::Vertex] = vsID;
	info.ShaderIDs[graphics::ShaderType::Pixel] = psID;
	info.Type = graphics::MaterialType::Opaque;
	mtlID = content::CreateResourceFromBlob(&info, content::AssetType::Material);

	loadedMaterialIDs.emplace_back(mtlID);
}

#define LOADED_TEST_COUNT 1

void
AddRenderItem()
{
	v3 pos{ -3.f, -10.f, 90.f };
	if (loadedModelsCount == 1) pos = v3{ 0.f, -10.f, 10.f };
	v3 rot{ -90.f, 180.f, 0.f };
	v3 scale{ 1.f, 1.f, 1.f };
	ecs::component::LocalTransform lt{ {}, pos, rot, scale };
	ecs::component::WorldTransform wt{};
	ecs::component::RenderMaterial material{};
	ecs::component::RenderMesh mesh{};

	//const char* path{ TEST_MESH_PATH };
	const char* path{ TEST_IMPORTED_MESH_PATH };
	//const char* path{ TEST_BISTRO_MESH_PATH };
	std::filesystem::path modelPath{ path };

	editor::DropModelIntoScene(modelPath);

	//MeshTest planeMeshTest{};
	//planeMeshTest.MeshID = LoadMesh(path); //FIXME: this assumes 1 LOD
	//content::UploadedGeometryInfo uploadedGeometryInfo{ content::GetLastUploadedGeometryInfo() };
	//planeMeshTest.MeshID = uploadedGeometryInfo.GeometryContentID;

	//u32 submeshCount{ uploadedGeometryInfo.SubmeshCount };


	//if(mtlID == id::INVALID_ID) CreateMaterial();
	////u32 materialCount{ 1 };
	//u32* materials = new u32[submeshCount];
	//for (u32 i{ 0 }; i < submeshCount; ++i)
	//{
	//	materials[i] = mtlID;
	//}
	//meshTests.emplace_back(planeMeshTest);

	//mesh.MeshID = planeMeshTest.MeshID;
	//material.MaterialCount = 1;
	//material.MaterialIDs = &materials[0];

	//// create root entity
	//ecs::EntityData& entityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform,
	//	ecs::component::RenderMesh, ecs::component::RenderMaterial>(lt, wt, mesh, material) };
	//loadedEntities.emplace_back(entityData.id);

	////u32 renderItemCount{ mofu::content::GetSubmeshGpuIDCount(mesh.MeshID) };
	////assert(renderItemCount == materialCount);


	//u32 subEntities{ (u32)uploadedGeometryInfo.SubmeshGpuIDs.size() };
	//ecs::component::Parent parentEntity{ {}, entityData.id };
	//for (u32 i{ 1 }; i < subEntities; ++i)
	//{
	//	pos.x -= 0.25f;
	//	pos.y += 0.25f;
	//	lt.Position = pos;
	//	id_t meshId{ uploadedGeometryInfo.SubmeshGpuIDs[i] };
	//	//mesh.MeshID = planeMeshTest.MeshID; //TODO:
	//	mesh.MeshID = meshId;
	//	material.MaterialIDs = &materials[i];
	//	material.MaterialCount = 1;

	//	ecs::EntityData& e{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform,
	//		ecs::component::RenderMesh, ecs::component::RenderMaterial, ecs::component::Parent>(lt, wt, mesh, material, parentEntity)};

	//	loadedMeshesIDs.emplace_back(meshId);
	//	loadedEntities.emplace_back(e.id);
	//	++loadedModelsCount;
	//	meshTests.emplace_back(MeshTest{ meshId, e.id });
	//}

	////planeMeshTest.EntityID = ecs::Entity{ 0 };
	////planeMeshTest.EntityID = ecs::Entity{ entityData.id };
	//planeMeshTest.EntityID = ecs::Entity{ entityData.id };
	////if(loadedModelsCount == 0) planeMeshTest.EntityID = ecs::Entity{ entityData.id };
	////else if (loadedModelsCount == 1) planeMeshTest.EntityID = ecs::Entity{ 1 };
	////else if (loadedModelsCount == 2) planeMeshTest.EntityID = ecs::Entity{ 1 };

	////graphics::AddRenderItem(planeMeshTest.EntityID, planeMeshTest.MeshID, 1, materials);
	////++loadedModelsCount;
	////loadedModelsCount = 3;

	//loadedModelsCount = submeshCount;
}

u32
CreateTestRenderItems()
{
	AddRenderItem();
	//AddRenderItem();
	//AddRenderItem();

	return loadedModelsCount;
}

void 
GetRenderItemIDS(Vec<id_t>& outIds)
{
	for (u32 i{ 0 }; i < loadedModelsCount; ++i)
	{
		outIds[i] = meshTests[i].MeshID;
	}
}

void
InitializeRenderingTest()
{
	LoadShaders();
}

void
ShutdownRenderingTest()
{
	for (id_t id : loadedMeshesIDs)
	{
		content::DestroyResource(id, content::AssetType::Mesh);
	}
	for (id_t id : loadedMaterialIDs)
	{
		content::DestroyResource(id, content::AssetType::Material);
	}
	for (ecs::Entity entity : loadedEntities)
	{
		ecs::scene::DestroyEntity(entity);
	}
}