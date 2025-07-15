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

#include "../External/tracy/public/tracy/Tracy.hpp"

using namespace mofu;

id_t content::CreateResourceFromBlob(const void* const blob, content::AssetType::type resourceType);

struct TextureUsage
{
	enum Usage : u32
	{
		BaseColor = 0,
		Normal,
		MetallicRoughness,
		Emissive,
		AmbientOcclusion,

		Count
	};
};
id_t textureIDs[TextureUsage::Count]{};

id_t vsID{ id::INVALID_ID };
id_t psID{ id::INVALID_ID };
id_t mtlID{ id::INVALID_ID };

id_t texturedPsID{ id::INVALID_ID };
id_t texturedMaterialID{ id::INVALID_ID };

constexpr const char* TEST_MESH_ASSET_PATH{ "Assets/Generated/plane.geom" };
constexpr const char* TEST_MESH_PATH{ "Assets/Generated/planeModel.model" };
constexpr const char* TEST_IMPORTED_MESH_PATH{ "Assets/Generated/testmodel.model" };
constexpr const char* TEST_BISTRO_MESH_PATH{ "Assets/Generated/BistroInterior.model" };

constexpr const char* TEST_TEXTURE_PATH{ "Assets/Generated/testTextureEnginePacked.tex" };

constexpr const char* WHITE_TEXTURE{ "Assets/Generated/Textures/white_placeholder_texture.tex" };
constexpr const char* GRAY_TEXTURE{ "Assets/Generated/Textures/gray_placeholder_texture.tex" };
constexpr const char* BLACK_TEXTURE{ "Assets/Generated/Textures/black_placeholder_texture.tex" };
constexpr const char* ERROR_TEXTURE{ "Assets/Generated/Textures/eror_texture.tex" };

struct ModelData
{
	const char* MeshFile{ TEST_MESH_PATH };
	const char* BaseColor{};
	const char* Normal{};
	const char* MetallicRoughness{};
	const char* AO{};
	const char* Emissive{};
	u32 somewhereAroundSubmeshCount{ 1 };
	v3 Pos{ -3.f, -10.f, 90.f };
	quat Rot{ quatIndentity };
	v3 Scale{ 1.f, 1.f, 1.f };
};

constexpr ModelData CYBORG_MODEL{
	"Assets/Generated/cyborg.model",
	//"Assets/Generated/testroomwithmonkeys.model",
	//"Assets/Generated/campfire_scene_simplest.model",
	//TEST_MESH_PATH,
	"Assets/Generated/Body_B.tga.tex",
	"Assets/Generated/Body_N.tga.tex",
	"Assets/Generated/Body_Metal.tga.tex",
	"Assets/Generated/Body_E.tga.tex",
	"Assets/Generated/Body_AO.tga.tex",
	//TEST_TEXTURE_PATH,
	//TEST_TEXTURE_PATH,
	//TEST_TEXTURE_PATH,
	//TEST_TEXTURE_PATH,
	//TEST_TEXTURE_PATH, //TODO: placeholder textures
	10
};

constexpr ModelData BISTRO_INTERIOR_MODEL{
	"Assets/Generated/BistroInterior.model",
	"Assets/Generated/Body_B.tga.tex",
	"Assets/Generated/Body_N.tga.tex",
	"Assets/Generated/Body_Metal.tga.tex",
	"Assets/Generated/Body_E.tga.tex",
	"Assets/Generated/Body_AO.tga.tex",
	3000
};


struct MeshTest
{
	id_t MeshID{ id::INVALID_ID };
	ecs::Entity EntityID{ id::INVALID_ID };
};
Vec<MeshTest> meshTests{};

constexpr u32 OBJECT_COUNT{ 1 };
constexpr u32 MATERIAL_COUNT{ 1 }; //NOTE: ! equal to submesh idCount
constexpr u32 MAX_MATERIALS_PER_MODEL{ 1024 };

constexpr u32 pbrSpheresCount{ 18 };
id_t sphereModelIDs[pbrSpheresCount];
id_t pbrMaterialIDs[pbrSpheresCount];

u32 loadedModelsCount{ 0 };
u32 loadedTexturesCount{ 0 };

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

[[nodiscard]] id_t
LoadTexture(const char* path)
{
	if (!path) return { id::INVALID_ID };
	++loadedTexturesCount;
	return LoadAsset(path, content::AssetType::Texture);
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

	extraArgs.clear();
	defines[0] = L"TEXTURED_MTL=1";
	extraArgs.emplace_back(L"-D");
	extraArgs.emplace_back(defines[0]);
	pixelShaders.emplace_back(std::move(CompileShader(info, shaderPath, extraArgs)));
	assert(pixelShaders.back().get());

	vsID = content::AddShaderGroup(vertexShaderPtrs.data(), (u32)vertexShaderPtrs.size(), keys.data());
	psID = content::AddShaderGroup(pixelShaderPtrs, 1, &U32_INVALID_ID);

	pixelShaderPtrs[0] = pixelShaders[1].get();
	texturedPsID = content::AddShaderGroup(pixelShaderPtrs, 1, &U32_INVALID_ID);
}

void
CreateMaterials()
{
	assert(id::IsValid(vsID) && id::IsValid(psID) && id::IsValid(texturedPsID));

	graphics::MaterialInitInfo info{};
	info.ShaderIDs[graphics::ShaderType::Vertex] = vsID;
	info.ShaderIDs[graphics::ShaderType::Pixel] = psID;
	info.Type = graphics::MaterialType::Opaque;
	mtlID = content::CreateResourceFromBlob(&info, content::AssetType::Material);

	loadedMaterialIDs.emplace_back(mtlID);

	//memset(pbrMaterialIDs, 0xDD, sizeof(pbrMaterialIDs));
	//v2 metallicRoughness[_countof(pbrMaterialIDs)]{
	//	{0.f, 0.0f}, {0.f, 0.2f}, {0.f, 0.4f}, {0.f, 0.6f}, {0.f, 0.8f}, {0.f, 1.0f},
	//	{0.f, 0.f}, {0.2f, 0.f}, {0.4f, 0.f}, {0.6f, 0.f}, {0.8f, 0.f}, {1.0f, 0.f},
	//	{0.f, 0.f}, {0.2f, 0.2f}, {0.4f, 0.4f}, {0.6f, 0.6f}, {0.8f, 0.8f}, {1.0f, 1.0f}
	//};
	//graphics::MaterialSurface& surface{ info.Surface };
	//surface.BaseColor = { 0.5f, 0.5f, 0.5f, 1.f };
	//for (u32 i{ 0 }; i < _countof(pbrMaterialIDs); ++i)
	//{
	//	surface.Metallic = metallicRoughness[i].x;
	//	surface.Roughness = metallicRoughness[i].y;
	//	pbrMaterialIDs[i] = content::CreateResourceFromBlob(&info, content::AssetType::Material);
	//}

	info.ShaderIDs[graphics::ShaderType::Pixel] = texturedPsID;
	if (loadedTexturesCount != 0)
	{
		info.TextureCount = TextureUsage::Count; // NOTE: assuming one of every texture usage exists
		info.TextureIDs = &textureIDs[0];
		texturedMaterialID = content::CreateResourceFromBlob(&info, content::AssetType::Material);
	}
}

#define LOADED_TEST_COUNT 1

void
AddRenderItem()
{
	v3 pos{ -3.f, -10.f, 90.f };
	if (loadedModelsCount == 1) pos = v3{ 0.f, -10.f, 10.f };
	quat rot{ quatIndentity };
	v3 scale{ 1.f, 1.f, 1.f };
	ecs::component::LocalTransform lt{ {}, pos, rot, scale };
	ecs::component::WorldTransform wt{};
	ecs::component::RenderMaterial material{};
	ecs::component::RenderMesh mesh{};

	//const char* path{ TEST_MESH_PATH };
	//const char* path{ TEST_IMPORTED_MESH_PATH };
	//const char* path{ TEST_BISTRO_MESH_PATH };
	//std::filesystem::path modelPath{ path };

	ModelData modelData{ CYBORG_MODEL };
	//ModelData modelData{ BISTRO_INTERIOR_MODEL };

	memset(&textureIDs[0], 0xEE, _countof(textureIDs) * sizeof(id_t));
	//std::thread threads[]{
	//	std::thread{[modelData] {textureIDs[TextureUsage::BaseColor] = LoadTexture(modelData.BaseColor); }},
	//	std::thread{[modelData] {textureIDs[TextureUsage::Normal] = LoadTexture(modelData.Normal); }},
	//	std::thread{[modelData] {textureIDs[TextureUsage::Emissive] = LoadTexture(modelData.Emissive); }},
	//	std::thread{[modelData] {textureIDs[TextureUsage::MetallicRoughness] = LoadTexture(modelData.MetallicRoughness); }},
	//	std::thread{[modelData] {textureIDs[TextureUsage::AmbientOcclusion] = LoadTexture(modelData.AO); }},
	//};
	textureIDs[TextureUsage::BaseColor] = LoadTexture(modelData.BaseColor);
	textureIDs[TextureUsage::Normal] = LoadTexture(modelData.Normal);
	textureIDs[TextureUsage::Emissive] = LoadTexture(modelData.Emissive);
	textureIDs[TextureUsage::MetallicRoughness] = LoadTexture(modelData.MetallicRoughness);
	textureIDs[TextureUsage::AmbientOcclusion] = LoadTexture(modelData.AO);

	//for (auto& t : threads)
	//{
	//	t.join();
	//}

	LoadShaders();

	CreateMaterials();

	constexpr bool USE_TEXTURES{ true };

	if (USE_TEXTURES)
	{
		u32* texturedMaterials = new u32[modelData.somewhereAroundSubmeshCount];
		for (u32 i{ 0 }; i < modelData.somewhereAroundSubmeshCount; ++i)
		{
			texturedMaterials[i] = texturedMaterialID;
		}
		editor::assets::DropModelIntoScene(modelData.MeshFile, texturedMaterials);
	}
	else
	{
		u32* materials = new u32[modelData.somewhereAroundSubmeshCount];
		for (u32 i{ 0 }; i < modelData.somewhereAroundSubmeshCount; ++i)
		{
			materials[i] = mtlID;
		}
		editor::assets::DropModelIntoScene(modelData.MeshFile, materials);
	}

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