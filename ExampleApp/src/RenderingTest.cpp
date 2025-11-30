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

u64 skySkyboxHandle{ 7733417588518510484 };

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

		f32 ambientLightIntensity{ 0.198f };
		id_t diffuseIBL{ content::assets::CreateResourceFromHandle(handles.DiffuseHandle) };
		id_t specularIBL{ content::assets::CreateResourceFromHandle(handles.SpecularHandle) };
		id_t BRDFLutIBL{ content::assets::CreateResourceFromHandle(handles.BrdfLutHandle) };
		id_t SkyboxHandle{ content::assets::CreateResourceFromHandle(skyboxHandle) };
		graphics::light::AddAmbientLight(lightSetOne, { ambientLightIntensity, diffuseIBL, specularIBL, BRDFLutIBL, SkyboxHandle });
	}
	else
	{
		f32 ambientLightIntensity{ 0.198f };
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