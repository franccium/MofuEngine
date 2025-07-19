#include "ResourceCreation.h"
#include <array>
#include "Graphics/Renderer.h"
#include "Content/EditorContentManager.h"

namespace mofu::content {
namespace {

Vec<id_t> geometryItemIDs{};
std::mutex geometryMutex{};

util::FreeList<std::unordered_map<u32, std::unique_ptr<u8[]>>> shaderGroups{}; // a free list of key-shader maps
std::mutex shaderMutex{};

//TODO: bad idea
UploadedGeometryInfo lastUploadedGeometryInfo{ id::INVALID_ID, 0, {} };

u32
GetGeometryHierarchyBufferSize(const void* const blob)
{
	assert(blob);
	util::BlobStreamReader reader{ (const u8*)blob };
	const u32 lodCount{ reader.Read<u32>() };
	assert(lodCount);
	constexpr u32 su32{ sizeof(u32) };
	// add size of lodCount, thresholds and lodOffsets to the size of the hierarchy
	u32 totalSize{ su32 + (sizeof(f32) + sizeof(content::LodOffset)) * lodCount };

	for (u32 lodIdx{ 0 }; lodIdx < lodCount; ++lodIdx)
	{
		// skip lod threshold
		reader.Skip(sizeof(f32));
		// add size of gpuIDs
		totalSize += sizeof(id_t) * reader.Read<u32>();
		// skip sumbesh data and go to the next LOD
		reader.Skip(reader.Read<u32>());
	}
	return totalSize;
}

Vec<UploadedGeometryInfo>
CreateGeometryItem(const void* const blob)
{
	Vec<UploadedGeometryInfo> info{};

	assert(blob);
	const u32 hierarchySize{ GetGeometryHierarchyBufferSize(blob) };

	util::BlobStreamReader reader{ (const u8*)blob };
	const u32 lodCount{ reader.Read<u32>() };
	assert(lodCount);

	for (u32 lodIdx{ 0 }; lodIdx < lodCount; ++lodIdx)
	{
		// upload all submeshes of the LOD and create an entity hierarchy, each submesh with its material for one entity
		//stream.Thresholds()[lodIdx] = reader.Read<f32>();
		reader.Skip(sizeof(f32));
		const u32 submeshCount{ reader.Read<u32>() };
		assert(submeshCount < (1 << 16));
		//stream.LODOffsets()[lodIdx] = { submeshIndex, (u16)idCount };
		reader.Skip(sizeof(f32)); // skip size of submeshes
		
		u32 submeshIndex{ 0 };
		id_t* const submeshGpuIDs = new id_t[submeshCount];

		for (u32 idIdx{ 0 }; idIdx < submeshCount; ++idIdx)
		{
			const u8* at{ reader.Position() };
			// upload the submesh to the GPU
			u32 id{ graphics::AddSubmesh(at) };
			submeshGpuIDs[submeshIndex++] = id;

			reader.Skip((u32)(at - reader.Position())); // go to the next submesh
		}

		//TODO: make it lock less
		std::lock_guard lock{ geometryMutex };
		for (u32 i{ 0 }; i < submeshCount; ++i)
		{
			assert(id::IsValid(submeshGpuIDs[i]));
			geometryItemIDs.emplace_back(submeshGpuIDs[i]);
		}
		assert(id::IsValid(submeshGpuIDs[0]));
		lastUploadedGeometryInfo = {};
		lastUploadedGeometryInfo.GeometryContentID = submeshGpuIDs[0];
		lastUploadedGeometryInfo.SubmeshCount = submeshCount;
		lastUploadedGeometryInfo.SubmeshGpuIDs.resize(submeshCount);
		std::copy(submeshGpuIDs, submeshGpuIDs + submeshCount, lastUploadedGeometryInfo.SubmeshGpuIDs.begin());

		info.emplace_back(lastUploadedGeometryInfo);
	}


	//assert([&]() {
	//	f32 previousThreshold{ stream.Thresholds()[0] };
	//	for (u32 i{ 1 }; i < lodCount; ++i)
	//	{
	//		if (stream.Thresholds()[i] < previousThreshold) return false;
	//		previousThreshold = stream.Thresholds()[i];
	//	}
	//	}());

	static_assert(alignof(void*) > 2, "The least significant bit in the pointer has to be zero for the single mesh marker implementation");
	/*std::lock_guard lock{ geometryMutex };
	id_t geometryID{ geometryHierarchies.add(hierarchyBuffer) };
	lastUploadedGeometryInfo.GeometryContentID = geometryID;
	lastUploadedGeometryInfo.SubmeshCount = submeshIndex;
	lastUploadedGeometryInfo.SubmeshGpuIDs.resize(submeshIndex);
	std::copy(gpuIDs, gpuIDs + submeshIndex, lastUploadedGeometryInfo.SubmeshGpuIDs.begin());*/

	return info;
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
	CreateGeometryItem(blob);
	return lastUploadedGeometryInfo.SubmeshGpuIDs[0];
}

id_t
CreateTextureResource(const void* const blob)
{
	assert(blob);
	id_t texId{ graphics::AddTexture((const u8* const)blob) };
	return texId;
}

id_t
CreateMaterialResource(const void* const blob)
{
	assert(blob);
	return graphics::AddMaterial(*(const graphics::MaterialInitInfo* const)blob);
}

void
DestroyGeometryResource(id_t id)
{
	// free the allocated block of memory, unloading all submeshes with it
	std::lock_guard lock{ geometryMutex };
	graphics::RemoveSubmesh(id);
}

void
DestroyMaterialResource(id_t id)
{
	graphics::RemoveMaterial(id);
}

void
DestroyTextureResource([[maybe_unused]] id_t id)
{
	assert(false);
}

#pragma region not_implemented
id_t
CreateUnknown([[maybe_unused]] const void* const blob)
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
	id_t resourceId{ resourceCreators[resourceType](blob) };
	return resourceId;
}
//
//id_t
//CreateResourceFromBlobWithHandle(const void* const blob, AssetType::type resourceType, AssetHandle handle)
//{
//	assert(blob);
//	id_t resourceId{ resourceCreators[resourceType](blob) };
//	content::assets::PairAssetWithResource(handle, resourceId, resourceType);
//	return resourceId;
//}

void
DestroyResource(id_t resourceId, AssetType::type resourceType)
{
	assert(id::IsValid(resourceId));
	resourceDestructors[resourceType](resourceId);
}

void
GetLODOffsets(const id_t* const geometryIDs, const f32* const thresholds, u32 idCount, Vec<LodOffset>& offsets)
{
	assert(geometryIDs && thresholds && idCount);
	assert(offsets.empty());

	std::lock_guard lock{ geometryMutex };
	//TODO:
}

// NOTE: expects shaders to be an array of pointers to compiled shaders
// NOTE: the editor is responsible for making sure there aren't any duplicate shaders
id_t
AddShaderGroup(const u8* const* shaders, u32 shaderCount, const u32* const keys)
{
	assert(shaders && shaderCount && keys);
	std::unordered_map<u32, std::unique_ptr<u8[]>> shaderGroup;
	for (u32 i{ 0 }; i < shaderCount; ++i)
	{
		assert(shaders[i]);
		const CompiledShaderPtr shaderPtr{ (const CompiledShaderPtr)shaders[i] };
		const u64 size{ shaderPtr->GetBufferSize() };
		std::unique_ptr<u8[]> shader{ std::make_unique<u8[]>(size) };
		memcpy(shader.get(), shaderPtr, size);
		shaderGroup[keys[i]] = std::move(shader);
	}

	std::lock_guard lock{ shaderMutex };
	return shaderGroups.add(std::move(shaderGroup));
}

void
RemoveShaderGroup(id_t groupID)
{
	assert(id::IsValid(groupID));
	std::lock_guard lock{ shaderMutex };
	shaderGroups.remove(groupID);
}

CompiledShaderPtr
GetShader(id_t groupID, u32 shaderKey)
{
	assert(id::IsValid(groupID));
	std::lock_guard lock{ shaderMutex };
	for (const auto& [key, value] : shaderGroups[groupID])
	{
		if (key == shaderKey) return (const CompiledShaderPtr)value.get();
	}

	assert(false);
	return nullptr;
}

//TODO: bad idea
UploadedGeometryInfo
GetLastUploadedGeometryInfo()
{
	return lastUploadedGeometryInfo;
}

}

//
//constexpr id_t
//GetGpuIDSingleMesh(u8* const meshPtr)
//{
//	assert((uintptr_t)meshPtr & SINGLE_MESH_MARKER);
//	static_assert(sizeof(uintptr_t) > sizeof(id_t));
//	constexpr u8 shiftBits{ (sizeof(uintptr_t) - sizeof(id_t)) << 3 };
//	return (((uintptr_t)meshPtr) >> shiftBits) & (uintptr_t)id::INVALID_ID;
//}
//
//bool
//ReadIsSingleMesh(const void* const blob)
//{
//	util::BlobStreamReader reader{ (const u8*)blob };
//	const u32 LodCount{ reader.Read<u32>() };
//	assert(LodCount);
//	if (LodCount > 1) return false;
//
//	reader.Skip(sizeof(f32));
//	const u32 submeshCount{ reader.Read<u32>() };
//	assert(submeshCount);
//	return submeshCount == 1;
//}
//
//u32
//GetGeometryHierarchyBufferSize(const void* const blob)
//{
//	assert(blob);
//	util::BlobStreamReader reader{ (const u8*)blob };
//	const u32 lodCount{ reader.Read<u32>() };
//	assert(lodCount);
//	constexpr u32 su32{ sizeof(u32) };
//	// add size of lodCount, thresholds and lodOffsets to the size of the hierarchy
//	u32 totalSize{ su32 + (sizeof(f32) + sizeof(content::LodOffset)) * lodCount };
//
//	for (u32 lodIdx{ 0 }; lodIdx < lodCount; ++lodIdx)
//	{
//		// skip lod threshold
//		reader.Skip(sizeof(f32));
//		// add size of gpuIDs
//		totalSize += sizeof(id_t) * reader.Read<u32>();
//		// skip sumbesh data and go to the next LOD
//		reader.Skip(reader.Read<u32>());
//	}
//	return totalSize;
//}
//
//id_t
//CreateSingleMesh(const void* const blob)
//{
//	util::BlobStreamReader reader{ (const u8*)blob };
//	// skip LODCount, LODThreshold, SubmeshCount and SizeOfSubmeshes
//	reader.Skip(sizeof(u32) + sizeof(f32) + sizeof(u32) + sizeof(u32));
//	const u8* submeshData{ reader.Position() };
//	const id_t gpuID{ graphics::AddSubmesh(submeshData) };
//
//	// create a fake pointer with 16-byte alignment and mark it as a single mesh gpuID
//	constexpr u8 bitShift{ (sizeof(uintptr_t) - sizeof(id_t)) << 3 };
//	u8* const singleGpuID{ (u8* const)((((uintptr_t)gpuID) << bitShift) | SINGLE_MESH_MARKER) };
//
//	std::lock_guard lock{ geometryMutex };
//	id_t geometryID{ geometryHierarchies.add(singleGpuID) };
//	lastUploadedGeometryInfo.GeometryContentID = geometryID;
//	lastUploadedGeometryInfo.SubmeshCount = 1;
//	lastUploadedGeometryInfo.SubmeshGpuIDs.resize(1);
//	lastUploadedGeometryInfo.SubmeshGpuIDs[0] = gpuID;
//	return geometryID;
//}
//
//id_t
//CreateMeshHierarchy(const void* const blob)
//{
//	assert(blob);
//	const u32 hierarchySize{ GetGeometryHierarchyBufferSize(blob) };
//	u8* const hierarchyBuffer{ (u8* const)malloc(hierarchySize) };
//
//	util::BlobStreamReader reader{ (const u8*)blob };
//	const u32 lodCount{ reader.Read<u32>() };
//	assert(lodCount);
//
//	GeometryHierarchyStream stream{ hierarchyBuffer, lodCount };
//	u16 submeshIndex{ 0 };
//	id_t* const gpuIDs{ stream.GpuIDs() };
//
//	for (u32 lodIdx{ 0 }; lodIdx < lodCount; ++lodIdx)
//	{
//		stream.Thresholds()[lodIdx] = reader.Read<f32>();
//		const u32 idCount{ reader.Read<u32>() };
//		assert(idCount < (1 << 16));
//		stream.LODOffsets()[lodIdx] = { submeshIndex, (u16)idCount };
//		reader.Skip(sizeof(f32)); // skip size of submeshes
//
//		for (u32 idIdx{ 0 }; idIdx < idCount; ++idIdx)
//		{
//			const u8* at{ reader.Position() };
//			// upload the submesh to the GPU
//			gpuIDs[submeshIndex++] = graphics::AddSubmesh(at);
//			reader.Skip((u32)(at - reader.Position())); // go to the next submesh
//		}
//	}
//
//	assert([&]() {
//		f32 previousThreshold{ stream.Thresholds()[0] };
//		for (u32 i{ 1 }; i < lodCount; ++i)
//		{
//			if (stream.Thresholds()[i] < previousThreshold) return false;
//			previousThreshold = stream.Thresholds()[i];
//		}
//	}());
//
//	static_assert(alignof(void*) > 2, "The least significant bit in the pointer has to be zero for the single mesh marker implementation");
//	std::lock_guard lock{ geometryMutex };
//	id_t geometryID{ geometryHierarchies.add(hierarchyBuffer) };
//	lastUploadedGeometryInfo.GeometryContentID = geometryID;
//	lastUploadedGeometryInfo.SubmeshCount = submeshIndex;
//	lastUploadedGeometryInfo.SubmeshGpuIDs.resize(submeshIndex);
//	std::copy(gpuIDs, gpuIDs + submeshIndex, lastUploadedGeometryInfo.SubmeshGpuIDs.begin());
//	return geometryID;
//}
//
///* expects data to contain :
//* struct {
//*  u32 LODCount,
//*  struct {
//*      f32 LODThreshold
//*      u32 submesh_count
//*      u32 size_of_submeshes
//*      struct {
//*              u32 elementSize, u32 vertexCount
//*              u32 indexCount, u32 elementType, u32 primitiveTopology
//*              u8 positions[sizeof(v3) * vertexCount], sizeof(positions) should be a multiple of 4 bytes
//*              u8 elements[elementSize * vertexCount], sizeof(elements) should be a multiple of 4 bytes
//*              u8 indices[index_size * indexCount]
//*          } submeshes[submesh_count]
//*      } meshLODs[LODCount]
//*  } geometry
//* 
//* Output format:
//* for a geometry hierarchy:
//* struct {
//*      u32 LODCount,
//*      f32 thresholds[LODCount]
//*      struct {
//*          u16 offset,
//*          u16 count
//*      } LODOffsets[LODCount]
//*      id_t gpu_ids[totalSubmeshCount]
//* } geometry_hierarchy
//*
//* for single LOD and submesh geometry:
//* (gpu_id << 32) | 0x01 
//*/
//id_t
//CreateGeometryResource(const void* const blob)
//{
//	return ReadIsSingleMesh(blob) ? CreateSingleMesh(blob) : CreateMeshHierarchy(blob);
//}
//
//void
//DestroyGeometryResource(id_t id)
//{
//	// free the allocated block of memory, unloading all submeshes with it
//	std::lock_guard lock{ geometryMutex };
//	u8* const hierarchyPtr{ geometryHierarchies[id] };
//	if ((uintptr_t)hierarchyPtr & SINGLE_MESH_MARKER)
//	{
//		// single mesh
//		graphics::RemoveSubmesh(GetGpuIDSingleMesh(hierarchyPtr));
//	}
//	else
//	{
//		GeometryHierarchyStream stream{ hierarchyPtr };
//		const u32 lodCount{ stream.LodCount() };
//		u32 id_index{ 0 };
//		for (u32 lod{ 0 }; lod < lodCount; ++lod)
//		{
//			for (u32 i{ 0 }; i < stream.LODOffsets()[lod].Count; ++i)
//			{
//				// unload all buffer resources under gpu_ids
//				graphics::RemoveSubmesh(stream.GpuIDs()[id_index++]);
//			}
//		}
//		free(hierarchyPtr);
//	}
//}
//
//id_t
//CreateMaterialResource(const void* const blob)
//{
//	assert(blob);
//	return graphics::AddMaterial(*(const graphics::MaterialInitInfo* const)blob);
//}
//
//void
//DestroyMaterialResource(id_t id)
//{
//	graphics::RemoveMaterial(id);
//}
//
//#pragma region not_implemented
//id_t 
//CreateUnknown([[maybe_unused]] const void* const blob)
//{
//	assert(false);
//	return id::INVALID_ID;
//}
//
//id_t
//CreateTextureResource([[maybe_unused]] const void* const blob)
//{
//	assert(false);
//	return id::INVALID_ID;
//}
//
//id_t
//CreateAnimationResource([[maybe_unused]] const void* const blob)
//{
//	assert(false);
//	return id::INVALID_ID;
//}
//
//id_t
//CreateAudioResource([[maybe_unused]] const void* const blob)
//{
//	assert(false);
//	return id::INVALID_ID;
//}
//
//id_t
//CreateSkeletonResource([[maybe_unused]] const void* const blob)
//{
//	assert(false);
//	return id::INVALID_ID;
//}
//
//void
//DestroyUnknown([[maybe_unused]] id_t id)
//{
//	assert(false);
//}
//
//void
//DestroyTextureResource([[maybe_unused]] id_t id)
//{
//	assert(false);
//}
//
//void
//DestroyAnimationResource([[maybe_unused]] id_t id)
//{
//	assert(false);
//}
//
//void
//DestroyAudioResource([[maybe_unused]] id_t id)
//{
//	assert(false);
//}
//
//void
//DestroySkeletonResource([[maybe_unused]] id_t id)
//{
//	assert(false);
//}
//#pragma endregion
//
//using ResourceCreator = id_t(*)(const void* const blob);
//constexpr std::array<ResourceCreator, AssetType::Count> resourceCreators{
//	CreateUnknown,
//	CreateGeometryResource,
//	CreateTextureResource,
//	CreateAnimationResource,
//	CreateAudioResource,
//	CreateMaterialResource,
//	CreateSkeletonResource,
//};
//static_assert(resourceCreators.size() == AssetType::Count, "Resource creator array size mismatch");
//using ResourceDestructor = void(*)(id_t id);
//constexpr std::array<ResourceDestructor, AssetType::Count> resourceDestructors{
//	DestroyUnknown,
//	DestroyGeometryResource,
//	DestroyTextureResource,
//	DestroyAnimationResource,
//	DestroyAudioResource,
//	DestroyMaterialResource,
//	DestroySkeletonResource,
//};
//static_assert(resourceDestructors.size() == AssetType::Count, "Resource destructor array size mismatch");
//
//} // anonymous namespace
//
//id_t
//CreateResourceFromBlob(const void* const blob, AssetType::type resourceType)
//{
//	assert(blob);
//	return resourceCreators[resourceType](blob);
//}
//
//void
//DestroyResource(id_t resourceId, AssetType::type resourceType)
//{
//	assert(id::IsValid(resourceId));
//	resourceDestructors[resourceType](resourceId);
//}
//
//u32 GetSubmeshGpuIDCount(id_t geometryContentID)
//{
//	std::lock_guard{ geometryMutex };
//	u8* const hierarchyPtr{ geometryHierarchies[geometryContentID] };
//	if ((uintptr_t)hierarchyPtr & SINGLE_MESH_MARKER)
//	{
//		return 1;
//	}
//	else
//	{
//		GeometryHierarchyStream stream{ hierarchyPtr };
//		return stream.LODOffsets()[0].Count;
//	}
//}
//
//void
//GetSubmeshGpuIDs(id_t geometryContentID, u32 submeshIndex, u32 idCount, id_t* const outGpuIDs, u32 counter)
//{
//	std::lock_guard lock{ geometryMutex };
//	u8* const hierarchyPtr{ geometryHierarchies[geometryContentID] };
//	//u8* const hierarchyPtr{ geometryHierarchies[0] };
//	if ((uintptr_t)hierarchyPtr & SINGLE_MESH_MARKER)
//	{
//		assert(idCount == 1);
//		*outGpuIDs = GetGpuIDSingleMesh(hierarchyPtr);
//	}
//	else
//	{
//		GeometryHierarchyStream stream{ hierarchyPtr };
//		//memcpy(outGpuIDs, &stream.GpuIDs()[counter], sizeof(id_t) * idCount); //TODO: testing with [counter
//		memcpy(outGpuIDs, &stream.GpuIDs()[submeshIndex], sizeof(id_t) * idCount);
//	}
//}
//
//void 
//GetLODOffsets(const id_t* const geometryIDs, const f32* const thresholds, u32 idCount, Vec<LodOffset>& offsets)
//{
//	assert(geometryIDs && thresholds && idCount);
//	assert(offsets.empty());
//
//	std::lock_guard lock{ geometryMutex };
//	for (u32 i{ 0 }; i < idCount; ++i)
//	{
//		u8* const hierarchyPtr{ geometryHierarchies[geometryIDs[i]] };
//		if ((uintptr_t)hierarchyPtr & SINGLE_MESH_MARKER)
//		{
//			offsets.emplace_back(LodOffset{ 0,1 });
//		}
//		else
//		{
//			GeometryHierarchyStream stream{ hierarchyPtr };
//			const u32 lod{ stream.LODFromThreshold(thresholds[i]) };
//			offsets.emplace_back(stream.LODOffsets()[lod]);
//		}
//	}
//}
//
//// NOTE: expects shaders to be an array of pointers to compiled shaders
//// NOTE: the editor is responsible for making sure there aren't any duplicate shaders
//id_t 
//AddShaderGroup(const u8* const* shaders, u32 shaderCount, const u32* const keys)
//{
//	assert(shaders && shaderCount && keys);
//	std::unordered_map<u32, std::unique_ptr<u8[]>> shaderGroup;
//	for (u32 i{ 0 }; i < shaderCount; ++i)
//	{
//		assert(shaders[i]);
//		const CompiledShaderPtr shaderPtr{ (const CompiledShaderPtr)shaders[i] };
//		const u64 size{ shaderPtr->GetBufferSize() };
//		std::unique_ptr<u8[]> shader{ std::make_unique<u8[]>(size) };
//		memcpy(shader.get(), shaderPtr, size);
//		shaderGroup[keys[i]] = std::move(shader);
//	}
//
//	std::lock_guard lock{ shaderMutex };
//	return shaderGroups.add(std::move(shaderGroup));
//}
//
//void
//RemoveShaderGroup(id_t groupID)
//{
//	assert(id::IsValid(groupID));
//	std::lock_guard lock{ shaderMutex };
//	shaderGroups.remove(groupID);
//}
//
//CompiledShaderPtr 
//GetShader(id_t groupID, u32 shaderKey)
//{
//	assert(id::IsValid(groupID));
//	std::lock_guard lock{ shaderMutex };
//	for (const auto& [key, value] : shaderGroups[groupID])
//	{
//		if (key == shaderKey) return (const CompiledShaderPtr)value.get();
//	}
//
//	assert(false);
//	return nullptr;
//}
//
////TODO: bad idea
//UploadedGeometryInfo 
//GetLastUploadedGeometryInfo()
//{
//	return lastUploadedGeometryInfo;
//}
//
//}