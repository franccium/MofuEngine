#include "ResourceCreation.h"
#include <array>
#include "Graphics/Renderer.h"

namespace mofu::content {
namespace {
util::FreeList<u8*> geometryHierarchies{};
std::mutex geometryMutex{};
// indicates than an element in geometryHierarchies is a single mesh (a gpuID, not a ptr)
constexpr u8 SINGLE_MESH_MARKER{ (uintptr_t)0x1 };

bool
ReadIsSingleMesh(const void* const blob)
{
	util::BlobStreamReader reader{ (const u8*)blob };
	const u32 LodCount{ reader.Read<u32>() };
	assert(LodCount);
	if (LodCount > 1) return false;

	reader.Skip(sizeof(f32));
	const u32 submeshCount{ reader.Read<u32>() };
	assert(submeshCount);
	return submeshCount == 1;
}

id_t
CreateSingleMesh(const void* const blob)
{
	util::BlobStreamReader reader{ (const u8*)blob };
	// skip LODCount, LODThreshold, SubmeshCount and SizeOfSubmeshes
	reader.Skip(sizeof(u32) + sizeof(f32) + sizeof(u32) + sizeof(u32));
	const u8* submeshData{ reader.Position() };
	const id_t gpuID{ graphics::AddSubmesh(submeshData) };

	// create a fake pointer with 16-byte alignment and mark it as a single mesh gpuID
	constexpr u8 bitShift{ (sizeof(uintptr_t) - sizeof(id_t)) << 3 };
	u8* const singleGpuID{ (u8* const)((((uintptr_t)gpuID) << bitShift) | SINGLE_MESH_MARKER) };

	std::lock_guard lock{ geometryMutex };
	return geometryHierarchies.add(singleGpuID);
}

/* expects data to contain :
* struct {
*  u32 LODCount,
*  struct {
*      f32 LODThreshold
*      u32 submesh_count
*      u32 size_of_submeshes
*      struct {
*              u32 elementSize, u32 vertexCount
*              u32 indexCount, u32 elementType, u32 primitiveTopology
*              u8 positions[sizeof(v3) * vertexCount], sizeof(positions) should be a multiple of 4 bytes
*              u8 elements[elementSize * vertexCount], sizeof(elements) should be a multiple of 4 bytes
*              u8 indices[index_size * indexCount]
*          } submeshes[submesh_count]
*      } meshLODs[LODCount]
*  } geometry
* 
* Output format:
* for a geometry hierarchy:
* struct {
*      u32 LODCount,
*      f32 thresholds[LODCount]
*      struct {
*          u16 offset,
*          u16 count
*      } LODOffsets[LODCount]
*      id_t gpu_ids[totalSubmeshCount]
* } geometry_hierarchy
*
* for single LOD and submesh geometry:
* (gpu_id << 32) | 0x01 
*/
id_t
CreateGeometryResource(const void* const blob)
{
	return CreateSingleMesh(blob);
}

void
DestroyGeometryResource(id_t id)
{

}

#pragma region not_implemented
id_t 
CreateUnknown([[maybe_unused]] const void* const blob)
{
	assert(false);
	return id::INVALID_ID;
}

id_t
CreateTextureResource([[maybe_unused]] const void* const blob)
{
	assert(false);
	return id::INVALID_ID;
}

id_t
CreateAnimationResource([[maybe_unused]] const void* const blob)
{
	assert(false);
	return id::INVALID_ID;
}

id_t
CreateAudioResource([[maybe_unused]] const void* const blob)
{
	assert(false);
	return id::INVALID_ID;
}

id_t
CreateMaterialResource([[maybe_unused]] const void* const blob)
{
	assert(false);
	return id::INVALID_ID;
}

id_t
CreateSkeletonResource([[maybe_unused]] const void* const blob)
{
	assert(false);
	return id::INVALID_ID;
}

void
DestroyUnknown([[maybe_unused]] id_t id)
{
	assert(false);
}

void
DestroyTextureResource([[maybe_unused]] id_t id)
{
	assert(false);
}

void
DestroyAnimationResource([[maybe_unused]] id_t id)
{
	assert(false);
}

void
DestroyAudioResource([[maybe_unused]] id_t id)
{
	assert(false);
}

void
DestroyMaterialResource([[maybe_unused]] id_t id)
{
	assert(false);
}

void
DestroySkeletonResource([[maybe_unused]] id_t id)
{
	assert(false);
}
#pragma endregion

using ResourceCreator = id_t(*)(const void* const blob);
constexpr std::array<ResourceCreator, AssetType::Count> resourceCreators{
	CreateUnknown,
	CreateGeometryResource,
	CreateTextureResource,
	CreateAnimationResource,
	CreateAudioResource,
	CreateMaterialResource,
	CreateSkeletonResource,
};
static_assert(resourceCreators.size() == AssetType::Count, "Resource creator array size mismatch");
using ResourceDestructor = void(*)(id_t id);
constexpr std::array<ResourceDestructor, AssetType::Count> resourceDestructors{
	DestroyUnknown,
	DestroyGeometryResource,
	DestroyTextureResource,
	DestroyAnimationResource,
	DestroyAudioResource,
	DestroyMaterialResource,
	DestroySkeletonResource,
};
static_assert(resourceDestructors.size() == AssetType::Count, "Resource destructor array size mismatch");

} // anonymous namespace

id_t
CreateResourceFromBlob(const void* const blob, AssetType::type resourceType)
{
	assert(blob);
	return resourceCreators[resourceType](blob);
}

void
DestroyResource(id_t resourceId, AssetType::type resourceType)
{
	assert(id::IsValid(resourceId));
	resourceDestructors[resourceType](resourceId);
}
}