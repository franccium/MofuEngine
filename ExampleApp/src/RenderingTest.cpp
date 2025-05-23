#include "CommonHeaders.h"
#include <filesystem>
#include "ECS/Entity.h"
#include "Graphics/GeometryData.h"
#include "Graphics/Renderer.h"
#include "Content/ContentManagement.h"
#include "Content/ResourceCreation.h"
#include "Content/ShaderCompilation.h"

using namespace mofu;

id_t content::CreateResourceFromBlob(const void* const blob, content::AssetType::type resourceType);

struct MeshTest
{
	id_t MeshID{ id::INVALID_ID };
	ecs::entity_id EntityID{ id::INVALID_ID };
};
MeshTest planeMeshTest{};

id_t vsID{ id::INVALID_ID };
id_t psID{ id::INVALID_ID };
id_t mtlID{ id::INVALID_ID };

constexpr const char* TEST_MESH_ASSET_PATH{ "Assets/Generated/plane.geom" };
constexpr const char* TEST_MESH_PATH{ "Assets/Generated/planeModel.model" };

constexpr u32 OBJECT_COUNT{ 1 };
constexpr u32 MATERIAL_COUNT{ 1 }; //NOTE: ! equal to submesh idCount
constexpr u32 MAX_MATERIALS_PER_MODEL{ 1024 };

u32 loadedModelsCount{ 0 };

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


}

u32
CreateTestRenderItems()
{
	u32 count{ 1 };
	planeMeshTest.MeshID = LoadMesh(TEST_MESH_PATH);
	planeMeshTest.EntityID = (ecs::entity_id)0;
	CreateMaterial();
	id_t materials[MAX_MATERIALS_PER_MODEL]{};
	id_t* materialIDs;
	u32 materialCount{ 1 };
	for (u32 i{ 0 }; i < materialCount; ++i)
	{
		materials[i] = mtlID;
	}
	materialIDs = materials;


	graphics::AddRenderItem(planeMeshTest.EntityID, planeMeshTest.MeshID, 1, &planeMeshTest.MeshID);

	++loadedModelsCount;

	return count;
}

void 
GetRenderItemIDS(Vec<id_t>& outIds)
{
	outIds[0] = planeMeshTest.MeshID;
}

void
InitializeRenderingTest()
{
	LoadShaders();
}

void
ShutdownRenderingTest()
{
	content::DestroyResource(planeMeshTest.MeshID, content::AssetType::Mesh);
}