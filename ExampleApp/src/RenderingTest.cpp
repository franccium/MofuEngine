#include "CommonHeaders.h"
#include <filesystem>
#include "ECS/Entity.h"
#include "Graphics/GeometryData.h"
#include "Graphics/Renderer.h"
#include "Content/ContentManagement.h"
#include "Content/ResourceCreation.h"

using namespace mofu;

id_t content::CreateResourceFromBlob(const void* const blob, content::AssetType::type resourceType);

struct MeshTest
{
	id_t MeshID{ id::INVALID_ID };
	ecs::entity_id EntityID{ id::INVALID_ID };
};

id_t vsID{ id::INVALID_ID };
id_t psID{ id::INVALID_ID };

constexpr const char* TEST_MESH_ASSET_PATH{ "Assets/Generated/plane.geom" };
constexpr const char* TEST_MESH_PATH{ "Assets/Generated/planeModel.model" };

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

}

void
SpawnTestMeshes()
{

}

void
InitializeRenderingTest()
{
	id_t meshID{ LoadMesh(TEST_MESH_PATH) };
	SpawnTestMeshes();
}

void
ShutdownRenderingTest()
{

}