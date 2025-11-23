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
#include "Editor/SceneEditorView.h"
#include "Graphics/Lights/Light.h"
#include "Content/EditorContentManager.h"
#if RAYTRACING
#include "Graphics/D3D12/D3D12Content/D3D12Geometry.h"
#endif

#include "../External/tracy/public/tracy/Tracy.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include "Physics/PhysicsCore.h"
#include "Physics/PhysicsLayers.h"
#include "ECS/Transform.h"
#include "ECS/Scene.h"
#ifdef NDEBUG
#define assert(expr)                                              \
    do {                                                             \
        if (!(expr)) {                                               \
            __debugbreak();                                          \
        }                                                            \
    } while (0);
#endif
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

constexpr const char* TEST_MESH_ASSET_PATH{ "Projects/TestProject/Assets/Generated/plane.geom" };
constexpr const char* TEST_MESH_PATH{ "Projects/TestProject/Assets/Generated/planeModel.model" };
constexpr const char* TEST_IMPORTED_MESH_PATH{ "Projects/TestProject/Assets/Generated/testmodel.model" };
constexpr const char* TEST_BISTRO_MESH_PATH{ "Projects/TestProject/Assets/Generated/BistroInterior.model" };

constexpr const char* TEST_TEXTURE_PATH{ "Projects/TestProject/Assets/Generated/testTextureEnginePacked.tex" };

constexpr const char* WHITE_TEXTURE{ "Projects/TestProject/Assets/Generated/Textures/white_placeholder_texture.tex" };
constexpr const char* GRAY_TEXTURE{ "Projects/TestProject/Assets/Generated/Textures/gray_placeholder_texture.tex" };
constexpr const char* BLACK_TEXTURE{ "Projects/TestProject/Assets/Generated/Textures/black_placeholder_texture.tex" };
constexpr const char* ERROR_TEXTURE{ "Projects/TestProject/Assets/Generated/Textures/eror_texture.tex" };

struct ModelData
{
	const char* MeshFile{ TEST_MESH_PATH };
	const char* BaseColor{};
	const char* Normal{};
	const char* MetallicRoughness{};
	const char* AO{};
	const char* Emissive{};
	u32 somewhereAroundSubmeshCount{ 1 };
	v3 Pos{ 0.f, 0.f, 0.f };
	quat Rot{ quatIndentity };
	v3 Scale{ 1.f, 1.f, 1.f };
};

constexpr ModelData CYBORG_MODEL{
	"Projects/TestProject/Assets/Generated/cyborg.model",
	//"Assets/Generated/testroomwithmonkeys.model",
	//"Assets/Generated/campfire_scene_simplest.model",
	//TEST_MESH_PATH,
	"Projects/TestProject/Assets/Generated/Body_B.tga.tex",
	"Projects/TestProject/Assets/Generated/Body_N.tga.tex",
	"Projects/TestProject/Assets/Generated/Body_Metal.tga.tex",
	"Projects/TestProject/Assets/Generated/Body_E.tga.tex",
	"Projects/TestProject/Assets/Generated/Body_AO.tga.tex",
	//TEST_TEXTURE_PATH,
	//TEST_TEXTURE_PATH,
	//TEST_TEXTURE_PATH,
	//TEST_TEXTURE_PATH,
	//TEST_TEXTURE_PATH, //TODO: placeholder textures
	10
};

constexpr ModelData BISTRO_INTERIOR_MODEL{
	"Projects/TestProject/Assets/Generated/BistroInterior.model",
	"Projects/TestProject/Assets/Generated/Body_B.tga.tex",
	"Projects/TestProject/Assets/Generated/Body_N.tga.tex",
	"Projects/TestProject/Assets/Generated/Body_Metal.tga.tex",
	"Projects/TestProject/Assets/Generated/Body_E.tga.tex",
	"Projects/TestProject/Assets/Generated/Body_AO.tga.tex",
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

//TODO: get rid of this
u64 diffuseIBLHandle{ 2947166161171442696 };
u64 specularIBLHandle{ 15753239102389846408 };
u64 brdfLutHandle{ 6591591707561885939 };
u64 skyboxHandle{ 1764144365702082788 };


//u64 diffuseIBLHandle{ 4366106093611776610 };
//u64 specularIBLHandle{ 10035983706911181120 };
//u64 brdfLutHandle{ 10865738213553409319 };
//u64 skyboxHandle{ 11277431815226102038 };

u64 skySkyboxHandle{ 7628588064764004896 };

// meadow
//u64 diffuseIBLHandle{ 3375815875843134095 };
//u64 specularIBLHandle{ 2879393416251087236 };
//u64 brdfLutHandle{ 6591591707561885939 };
//u64 skyboxHandle{ 1764144365702082788 };

constexpr f32 INV_RAND_MAX{ 1.f / RAND_MAX };
f32 Random(f32 min = 0.f) { return std::max(min, rand() * INV_RAND_MAX); }

#define DO_RANDOM_LIGHTS 0

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
	shaders::ShaderFileInfo info{};
	info.File = "TestShader.hlsl";
	info.EntryPoint = "TestShaderVS";
	info.Type = shaders::ShaderType::Vertex;
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

	info.EntryPoint = "TestShaderPS";
	info.Type = shaders::ShaderType::Pixel;
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
	info.ShaderIDs[shaders::ShaderType::Vertex] = vsID;
	info.ShaderIDs[shaders::ShaderType::Pixel] = psID;
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

	info.ShaderIDs[shaders::ShaderType::Pixel] = texturedPsID;
	if (loadedTexturesCount != 0)
	{
		info.TextureCount = TextureUsage::Count; // NOTE: assuming one of every texture usage exists
		info.TextureIDs = &textureIDs[0];
		texturedMaterialID = content::CreateResourceFromBlob(&info, content::AssetType::Material);
	}
}

#define LOADED_TEST_COUNT 1

void
AddLights()
{
	u32 lightSetOne{ graphics::light::CreateLightSet() };

	bool createFromHandles{ true };
	if (createFromHandles)
	{
		content::AssetHandle skyboxHandle{ skySkyboxHandle };
		content::AssetPtr skybox{ content::assets::GetAsset(skyboxHandle) };
		content::assets::AmbientLightHandles handles{ content::assets::GetAmbientLightHandles(skyboxHandle) };
		//assert(content::IsValid(skybox->AdditionalData2));
		content::AssetHandle diffuseHandle{ content::assets::GetIBLRelatedHandle(skyboxHandle) };
		content::AssetHandle specularHandle{ content::assets::GetIBLRelatedHandle(diffuseHandle) };
		content::AssetHandle brdfLUTHandle{ content::assets::GetIBLRelatedHandle(specularHandle) };
		
		
		////content::AssetHandle diffuseHandle{ skybox->AdditionalData2 };
		//content::AssetPtr diffuseAsset{ content::assets::GetAsset(diffuseHandle) };
		//assert(diffuseAsset);
		////content::AssetHandle specularHandle{ diffuseAsset->AdditionalData2 };
		//content::AssetPtr specularAsset{ content::assets::GetAsset(specularHandle) };
		//assert(specularAsset);
		////content::AssetHandle brdfLUTHandle{ specularAsset->AdditionalData2 };
		//content::AssetPtr brdfLUTAsset{ content::assets::GetAsset(brdfLUTHandle) };
		//assert(brdfLUTAsset);

		f32 ambientLightIntensity{ 1.f };
		id_t diffuseIBL{ content::assets::CreateResourceFromHandle(handles.DiffuseHandle) };
		id_t specularIBL{ content::assets::CreateResourceFromHandle(handles.SpecularHandle) };
		id_t BRDFLutIBL{ content::assets::CreateResourceFromHandle(handles.BrdfLutHandle) };
		id_t SkyboxHandle{ content::assets::CreateResourceFromHandle(skyboxHandle) };
		graphics::light::AddAmbientLight(lightSetOne, { ambientLightIntensity, diffuseIBL, specularIBL, BRDFLutIBL, SkyboxHandle });
	}
	else
	{
		f32 ambientLightIntensity{ 1.f };
		id_t diffuseIBL{ content::assets::CreateResourceFromHandle(diffuseIBLHandle) };
		id_t specularIBL{ content::assets::CreateResourceFromHandle(specularIBLHandle) };
		id_t BRDFLutIBL{ content::assets::CreateResourceFromHandle(brdfLutHandle) };
		id_t SkyboxHandle{ content::assets::CreateResourceFromHandle(skyboxHandle) };
		graphics::light::AddAmbientLight(lightSetOne, { ambientLightIntensity, diffuseIBL, specularIBL, BRDFLutIBL, SkyboxHandle });
	}

	

	ecs::component::LocalTransform lt{};
	ecs::component::WorldTransform wt{};

	ecs::component::Light light{};
	ecs::component::DirectionalLight dirLight{};

	ecs::component::PointLight pointLight{};
	ecs::component::SpotLight spotLight{};

	constexpr u32 DIR_LIGHT_COUNT{ 1 };
	v3 directions[DIR_LIGHT_COUNT]{ { 0.3f, -0.92f, 0.1f } };
	v3 colors[DIR_LIGHT_COUNT]{ { 1.f, 1.f, 1.f } };
	f32 intensities[DIR_LIGHT_COUNT]{ 1.f };
	bool enabled[DIR_LIGHT_COUNT]{ true };
	//constexpr u32 DIR_LIGHT_COUNT{ 4 };
	//v3 directions[DIR_LIGHT_COUNT]{ {-0.4f, 0.f, 1.f }, { -0.33f, -0.8f, -0.99f }, { 0.3f, 0.2f, 0.1f }, { 0.7f, 1.0f, 0.5f } };
	////v3 colors[DIR_LIGHT_COUNT]{ { 0.9f, 0.3f, 0.2f }, { 0.2f, 0.8f, 0.2f }, { 0.1f, 0.6f, 0.6f }, { 0.6f, 0.8f, 0.2f } };
	//v3 colors[DIR_LIGHT_COUNT]{ { 1.f, 1.f, 1.f }, { 1.f, 1.f, 1.f }, { 1.f, 1.f, 1.f }, { 1.f, 1.f, 1.f } };
	//f32 intensities[DIR_LIGHT_COUNT]{ 1.f, 1.f, 1.f, 1.f };
	//f32 intensities[DIR_LIGHT_COUNT]{ 0.f, 0.f, 0.f, 0.f };
	//bool enabled[DIR_LIGHT_COUNT]{ true, true, true, true };

	for (u32 i{ 0 }; i < DIR_LIGHT_COUNT; ++i)
	{
		dirLight.Color = colors[i];
		dirLight.Intensity = intensities[i];
		dirLight.Enabled = enabled[i];
		dirLight.Direction = directions[i];
		const auto& entityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::Parent, ecs::component::WorldTransform,
			ecs::component::Light, ecs::component::DirectionalLight>(lt, {}, wt, light, dirLight) };
		editor::AddEntityToSceneView(entityData.id);

		graphics::light::AddLightToLightSet(lightSetOne, entityData.id, graphics::light::LightType::Directional);
	}

	const auto& lightParent{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::Parent, ecs::component::WorldTransform>(lt, {}, wt) };
	ecs::Entity lightParentEntity{ lightParent.id };
	editor::AddEntityToSceneView(lightParentEntity);
	{
		constexpr u32 POINT_LIGHT_COUNT{ 8 };
		v3 positions[POINT_LIGHT_COUNT]{ { 0.9f, 0.3f, 0.2f }, { 0.2f, 0.8f, 0.2f }, { 0.0f, 0.6f, 0.6f }, { 0.6f, 0.8f, 0.2f },
			{ 0.9f, 0.9f, 0.2f }, { 0.6f, 0.8f, 0.3f }, { 0.3f, 0.6f, 0.1f }, { 0.8f, 0.2f, 0.0f } };
		v3 colors[POINT_LIGHT_COUNT]{ { 0.9f, 0.3f, 0.2f }, { 0.2f, 0.8f, 0.2f }, { 0.1f, 0.6f, 0.6f }, { 0.6f, 0.8f, 0.2f },
			{ 0.9f, 0.9f, 0.2f }, { 0.6f, 0.8f, 0.3f }, { 0.3f, 0.6f, 0.1f }, { 0.8f, 0.2f, 0.0f } };
		f32 intensities[POINT_LIGHT_COUNT]{ 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
		bool enabled[POINT_LIGHT_COUNT]{ true, true, true, true, true, true, true, true };
		for (u32 i{ 0 }; i < POINT_LIGHT_COUNT; ++i)
		{
			lt.Position = positions[i];
			pointLight.Color = colors[i];
			pointLight.Intensity = intensities[i];
			pointLight.Enabled = enabled[i];
			/*const auto& entityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::Parent, ecs::component::WorldTransform,
					ecs::component::Light, ecs::component::PointLight>(lt, {}, wt, light, pointLight) };*/
			const auto& entityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::Child, ecs::component::WorldTransform,
					ecs::component::Light, ecs::component::PointLight, ecs::component::CullableLight>(lt, {{}, lightParentEntity}, wt, light, pointLight, {}) };
			editor::AddEntityToSceneView(entityData.id);

			graphics::light::AddLightToLightSet(lightSetOne, entityData.id, graphics::light::LightType::Point);
		}
	}

	{
		const auto& entityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::Parent, ecs::component::WorldTransform,
				ecs::component::Light, ecs::component::SpotLight, ecs::component::CullableLight>(lt, {}, wt, light, spotLight, {}) };
		editor::AddEntityToSceneView(entityData.id);

		graphics::light::AddLightToLightSet(lightSetOne, entityData.id, graphics::light::LightType::Spot);
	}

#if DO_RANDOM_LIGHTS

	srand(17);

	constexpr i32 startZ = 0;
	constexpr i32 startX = 0;
	constexpr i32 startY = 0;

	constexpr f32 scatterScale{ 5.0f };
	constexpr v3 scale{ 1.f * scatterScale, 0.2f * scatterScale, 1.f * scatterScale };
	constexpr i32 dim{ 4 };
	for (i32 x{ -dim + startX }; x < dim + startX; ++x)
	{
		for (i32 y{ startY }; y < 10; ++y)
		{
			for (i32 z{ startZ }; z < dim + 5 + startZ; ++z)
			{
				v3 pos{ (f32)x * scale.x, (f32)y, (f32)z * scale.z };
				v3 rotEuler{ 3.14f, Random(), 0.f };

				lt.Position = pos;
				pointLight.Color = { Random(0.2f), Random(0.2f), Random(0.2f) };
				pointLight.Intensity = { Random(0.5f) };
				pointLight.Range = { Random(0.5f) * 8.f };
				pointLight.Enabled = true;
				const auto& entityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::Child, ecs::component::WorldTransform,
						ecs::component::Light, ecs::component::PointLight, 
					ecs::component::CullableLight>(lt, {{}, lightParentEntity}, wt, light, pointLight, {}) };
				editor::AddEntityToSceneView(entityData.id);

				graphics::light::AddLightToLightSet(lightSetOne, entityData.id, graphics::light::LightType::Point);
			}
		}
	}
#endif
}

void
AddRenderItem()
{
	ModelData modelData{ CYBORG_MODEL };
	v3 pos{ modelData.Pos };
	quat rot{ modelData.Rot };
	v3 scale{ modelData.Scale };
	ecs::component::LocalTransform lt{ {}, pos, rot, scale };
	ecs::component::WorldTransform wt{};
	ecs::component::RenderMaterial material{};
	ecs::component::RenderMesh mesh{};

	//const char* path{ TEST_MESH_PATH };
	//const char* path{ TEST_IMPORTED_MESH_PATH };
	//const char* path{ TEST_BISTRO_MESH_PATH };
	//std::filesystem::path modelPath{ path };

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


	content::AssetHandle meshAsset{ content::assets::DEFAULT_MESH_HANDLE };
	content::AssetHandle materialAsset{ content::assets::DEFAULT_MATERIAL_UNTEXTURED_HANDLE };

	ecs::component::RenderMesh mesh{};
	ecs::component::RenderMaterial mat{};
	mesh.MeshID = content::GetDefaultMesh();
	mesh.MeshAsset = meshAsset;
	mat.MaterialAsset = materialAsset;
	mat.MaterialCount = 1;
	mat.MaterialID = content::GetDefaultMaterial();

	ecs::component::LocalTransform transform{};
	transform.Position = { 0.f, -1.f, 21 };
#if RAYTRACING
	ecs::component::PathTraceable pt{};
	pt.MeshInfo = graphics::d3d12::content::geometry::MeshInfo{ graphics::d3d12::content::geometry::GetMeshInfo(mesh.MeshID) };

	//editor::AddPrefab("Projects/TestProject/Resources/Prefabs/a2.pre");
	//editor::AddPrefab("Projects/TestProject/Resources/Prefabs/three-cubes.pre");

	ecs::EntityData& entityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform,
		ecs::component::Parent, ecs::component::WorldTransform, ecs::component::RenderMesh, ecs::component::RenderMaterial, ecs::component::PathTraceable>(transform, {}, {},
			mesh, mat, pt)};
#else
	ecs::EntityData& entityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform,
		ecs::component::Parent, ecs::component::WorldTransform, ecs::component::RenderMesh, ecs::component::RenderMaterial>(transform, {}, {},
			mesh, mat) };
#endif

	///editor::AddPrefab("Projects/TestProject/Resources/Prefabs/a2.pre");
	ecs::component::RenderMesh& meshEd{ ecs::scene::GetComponent<ecs::component::RenderMesh>(entityData.id) };
	meshEd.RenderItemID = graphics::AddRenderItem(entityData.id, mesh.MeshID, mat.MaterialCount, mat.MaterialID);
	editor::AddEntityToSceneView(entityData.id);
	AddLights();

	const content::AssetHandle RT_CUBES{ 15519544575226091575 };
	//editor::AddPrefab("Projects/TestProject/Resources/Prefabs/three-cubes.pre");
	editor::AddPrefab("Projects/TestProject/Resources/Prefabs/boxestest.pre");
	const content::AssetHandle SUN_TEMPLE{ 13905473850964664605 };
	//editor::AddPrefab("Projects/TestProject/Resources/Prefabs/suntemple1.pre");


	/*for (u32 i{ 0 }; i < 3; ++i)
	{
		ecs::Entity e{ editor::AddPrefab("EditorAssets/Prefabs/pcube.pre") };
		ecs::scene::AddComponent<ecs::component::StaticObject>(e);
		
		JPH::BoxShape boxShape{ JPH::Vec3{1.f, 1.f, 1.f} };
		JPH::Shape* shape{ new JPH::BoxShape{JPH::Vec3{1.f, 1.f, 1.f}} };
		
		ecs::component::LocalTransform& lt{ ecs::scene::GetEntityComponent<ecs::component::LocalTransform>(e) };
		lt.Position.x += i * 3.f;
		lt.Position.y += i * 0.2f;
		JPH::RVec3 pos{ lt.Position.x, lt.Position.y, lt.Position.z };
		JPH::Quat rot{ lt.Rotation.x, lt.Rotation.y, lt.Rotation.z, lt.Rotation.w };

		physics::AddStaticBodyFromMesh()
	}*/

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