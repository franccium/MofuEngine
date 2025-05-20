#include "GeometryData.h"
#include "Utilities/IOStream.h"

namespace mofu::content {
namespace {

} // anonymous namespace

u64
GetMeshSize(const Mesh& m)
{
	constexpr u32 su32{ sizeof(u32) };
	const u64 vertexCount{ m.Vertices.size() };
	const u64 indexSize{ (vertexCount < (1 << 16)) ? sizeof(u16) : sizeof(u32) };
	const u64 indexBufferSize{ indexSize * m.Indices.size() };

	return {
		su32 + m.Name.size() + // mesh name length and room for the name string
		su32 + // mesh id
		su32 + // vertex element size (vertex size exluding the position element)
		su32 + // element type enum
		su32 + // number of vertices
		su32 + // index size (16b or 32b)
		su32 + // number of indices
		sizeof(f32) + // LOD threshold
		m.PositionBuffer.size() + // room for vertex positions
		m.ElementBuffer.size() + // room for vertices elements
		indexBufferSize // room for indices
	};
}

u64
GetMeshGroupSize(const MeshGroup& group)
{
	constexpr u64 su32{ sizeof(u32) };

	u64 size{ su32 + group.Name.size() + su32 };
	for (const auto& lod : group.LodGroups)
	{
		u64 lodSize{ su32 + lod.Name.size() + su32 };
		for (const auto& mesh : lod.Meshes)
		{
			lodSize += GetMeshSize(mesh);
		}
		size += lodSize;
	}
	return size;
}

/*
* Format:
* [u32] mesh name length
* [u32] mesh name
* [u32] mesh id
* [u32] vertex element size
* [u32] element type
* [u32] number of vertices
* [u32] index size (16b/32b)
* [u32] number of indices
* [f32] LOD threshold
* [u8] vertex positions
* [u8] vertex elements
* [u8] indices
*/
void 
PackMeshData(const Mesh& m, util::BlobStreamWriter& blob)
{
	blob.WriteStringWithLength(m.Name);
	blob.Write(m.LodID);

	const u32 elementSize{ (u32)GetVertexElementSize(m.ElementType) };
	blob.Write(elementSize);
	blob.Write((u32)m.ElementType);
	const u32 vertexCount{ (u32)m.Vertices.size() };
	blob.Write(vertexCount);

	const u32 indexSize{ (vertexCount < (1 << 16)) ? sizeof(u16) : sizeof(u32) };
	const u32 indexCount{ (u32)m.Indices.size() };
	blob.Write(indexSize);
	blob.Write(indexCount);
	blob.Write(m.LodThreshold);

	assert(m.PositionBuffer.size() == vertexCount * sizeof(v3));
	blob.WriteBytes(m.PositionBuffer.data(), m.PositionBuffer.size());
	assert(m.ElementBuffer.size() == vertexCount * elementSize);
	blob.WriteBytes(m.ElementBuffer.data(), m.ElementBuffer.size());

	const u32 indexBufferSize{ indexSize * indexCount };
	const u8* data{ (const u8*)m.Indices.data() };
	Vec<u16> indices{ indexCount };
	if (indexSize == sizeof(u16))
	{
		indices.resize(indexCount);
		for (u32 i{ 0 }; i < indexCount; ++i)
			indices[i] = (u16)m.Indices[i];
		data = (const u8*)indices.data();
	}
	blob.WriteBytes(data, indexBufferSize);
}

/*
* Format:
* [u32] MeshGroup name length
* [u32] MeshGroup name
* [u32] number of LODs
* [u32] LOD name length
* [u32] LOD name
* [u32] number of meshes
* [] mesh data
*/
void 
PackGeometryData(const MeshGroup& meshGroup, MeshGroupData& outData)
{
	const u64 groupSize{ GetMeshGroupSize(meshGroup) };
	outData.Buffer = new u8[groupSize];
	outData.BufferSize = groupSize;

	util::BlobStreamWriter blob{ outData.Buffer, groupSize };
	blob.WriteStringWithLength(meshGroup.Name);
	// number of LODs
	blob.Write((u32)meshGroup.LodGroups.size());

	for (const auto& lod : meshGroup.LodGroups)
	{
		blob.WriteStringWithLength(lod.Name);
		// number of meshes
		blob.Write((u32)lod.Meshes.size());

		for (const auto& m : lod.Meshes)
		{
			PackMeshData(m, blob);
		}
	}
	assert(blob.Offset() == groupSize);
}

}
