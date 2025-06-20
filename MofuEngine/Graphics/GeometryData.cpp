#include "GeometryData.h"
#include "Utilities/IOStream.h"
#include <filesystem>
#include <fstream>

using namespace DirectX;

namespace mofu::content {
namespace {

void
SplitMeshesByMaterial(MeshGroup& group)
{
	//TODO:
}

void 
CalculateNormals(Mesh& m)
{
	const u32 indexCount{ (u32)m.RawIndices.size() };
	m.Normals.resize(indexCount);

	for (u32 i{ 0 }; i < indexCount; i += 3)
	{
		const u32 idx0{ m.RawIndices[i] };
		const u32 idx1{ m.RawIndices[(i + 1)] };
		const u32 idx2{ m.RawIndices[(i + 2)] };	

		for (u32 v{ 0 }; v < 4; ++v)
		{
			v3& v0{ m.Positions[v] };
			u32 a{ 5 };
		}

		xmm v0{ XMLoadFloat3(&m.Positions[idx0]) };
		xmm v1{ XMLoadFloat3(&m.Positions[idx1]) };
		xmm v2{ XMLoadFloat3(&m.Positions[idx2]) };	

		xmm edge0{ v1 - v0 };
		xmm edge1{ v2 - v0 };
		xmm n{ XMVector3Normalize(XMVector3Cross(edge0, edge1)) };	

		v3 normal;
		XMStoreFloat3(&normal, n);
		m.Normals[i] = normal;	
		m.Normals[i + 1] = normal;
		m.Normals[i + 2] = normal;
	}
}

void
ProcessNormals(Mesh& m, f32 smoothingAngle)
{
	const f32 smoothingCos{ XMScalarCos(math::PI - smoothingAngle * math::PI / 180.f) };
	const bool isHardEdge{ XMScalarNearEqual(smoothingCos, 180.f, math::EPSILON) };
	const bool isSmoothEdge{ XMScalarNearEqual(smoothingAngle, 0.f, math::EPSILON) };

	const u32 indexCount{ (u32)m.RawIndices.size() };
	const u32 vertexCount{ (u32)m.Positions.size() };	

	assert(indexCount && vertexCount);
	m.Indices.resize(indexCount);

	Vec<Vec<u32>> vertexToIndex(vertexCount);
	for (u32 i{ 0 }; i < indexCount; ++i)
		vertexToIndex[m.RawIndices[i]].emplace_back(i);

	for (u32 i{ 0 }; i < vertexCount; ++i)
	{
		auto& refs{ vertexToIndex[i] };
		u32 refsCount{ (u32)refs.size() };

		for (u32 j{ 0 }; j < refsCount; ++j)
		{
			m.Indices[refs[j]] = (u32)m.Vertices.size();
			Vertex& v{ *m.Vertices.emplace_back() };	
			v.Position = m.Positions[m.RawIndices[refs[j]]];

			xmm n1{ XMLoadFloat3(&m.Normals[refs[j]]) };
			if (!isHardEdge)
			{
				for (u32 k{ j + 1 }; k < refsCount; ++k)
				{
					f32 nCos{ 0.f };
					xmm n2{ XMLoadFloat3(&m.Normals[refs[k]]) };
					if (!isSmoothEdge)
					{
						// assuming n2 is normalized, we don't divide by it's length, but n1's length can change in this iteration
						XMStoreFloat(&nCos, XMVector3Dot(n1, n2) * XMVector3ReciprocalLength(n1));
					}
					if (isSmoothEdge || nCos >= smoothingAngle)
					{
						n1 += n2;
						m.Indices[refs[k]] = m.Indices[refs[j]];
						refs.erase(refs.begin() + k);
						--k;
						--refsCount;
					}
				}
			}
			XMStoreFloat3(&v.Normal, XMVector3Normalize(n1));
		}
	}
}

void
CalculateMikkTSpace(Mesh& m)
{
	//TODO:
}

void
ProcessTangents(Mesh& m)
{
	//TODO:
}

[[nodiscard]] ElementType::type
DetermineElementType(Mesh& m)
{
	if (m.Normals.size())
	{
		if (m.UvSets.size() && m.UvSets[0].size())
		{
			return ElementType::StaticNormalTexture;
		}
		else
		{
			return ElementType::StaticNormal;
		}
	}
	else if (m.Colors.size())
	{
		return ElementType::StaticColor;
	}

	// other types...
	assert(false);
	return ElementType::PositionOnly;		
}

void
PackVertexData(Mesh& m)
{
	const u32 vertexCount{ (u32)m.Vertices.size() };
	assert(vertexCount);

	m.PositionBuffer.resize(vertexCount * sizeof(v3));
	v3* const posBuffer{ (v3* const)m.PositionBuffer.data() };	
	for (u32 i{ 0 }; i < vertexCount; ++i)
	{
		posBuffer[i] = m.Vertices[i].Position;
	}

	struct u16v2 { u16 x, y; };
	struct u8v3 { u8 x, y, z; };

	Vec<u8> tnSigns{ vertexCount };
	Vec<u16v2> normals{ vertexCount };
	Vec<u16v2> tangents{ vertexCount };

	if (m.ElementType & ElementType::StaticNormal)
	{
		// pack normals
		for (u32 i{ 0 }; i < vertexCount; ++i)
		{
			const Vertex& v{ m.Vertices[i] };
			tnSigns[i] = { (u8)((v.Normal.z > 0.f) << 2) };
			normals[i] = { (u16)math::PackFloat<16>(v.Normal.x, -1.f, 1.f), (u16)math::PackFloat<16>(v.Normal.y, -1.f, 1.f) };
		}

		if (m.ElementType & ElementType::StaticNormalTexture)
		{
			// pack tangents
			for (u32 i{ 0 }; i < vertexCount; ++i)
			{
				const Vertex& v{ m.Vertices[i] };
				tnSigns[i] |= (u8)((v.Tangent.w > 0.f) | ((v.Tangent.z > 0.f) << 1));
				tangents[i] = { (u16)math::PackFloat<16>(v.Tangent.x, -1.f, 1.f), (u16)math::PackFloat<16>(v.Tangent.y, -1.f, 1.f) };
			}
		}
	}

	// other types...

	m.ElementBuffer.resize(vertexCount * GetVertexElementSize(m.ElementType));
	switch (m.ElementType)
	{
	case ElementType::StaticNormal:
	{
		StaticNormal* const elementBuffer{ (StaticNormal* const)m.ElementBuffer.data() };
		for (u32 i{ 0 }; i < vertexCount; ++i)
		{
			Vertex& v{ m.Vertices[i] };
			elementBuffer[i] = { {v.Red, v.Green, v.Blue}, tnSigns[i], { normals[i].x, normals[i].y } };
		}
	}
	break;
	case ElementType::StaticNormalTexture:
	{
		StaticNormalTexture* const elementBuffer{ (StaticNormalTexture* const)m.ElementBuffer.data() };
		for (u32 i{ 0 }; i < vertexCount; ++i)
		{
			Vertex& v{ m.Vertices[i] };
			elementBuffer[i] = { {v.Red, v.Green, v.Blue}, tnSigns[i], { normals[i].x, normals[i].y }, {tangents[i].x, tangents[i].y}, v.UV };
		}
	}
	break;
	case ElementType::StaticColor:
	{
		StaticColor* const elementBuffer{ (StaticColor* const)m.ElementBuffer.data() };
		for (u32 i{ 0 }; i < vertexCount; ++i)
		{
			Vertex& v{ m.Vertices[i] };
			elementBuffer[i] = { {v.Red, v.Green, v.Blue}, {} };
		}
	}
	break;
	}
}

void
ProcessAndPackVertexData(Mesh& m, const GeometryImportSettings& settings)
{
	assert((m.RawIndices.size() % 3) == 0);
	if (settings.CalculateNormals || m.Normals.empty())
	{
		CalculateNormals(m);
	}

	ProcessNormals(m, settings.SmoothingAngle);

	if (settings.CalculateTangents || (m.Tangents.empty() && !m.UvSets.empty()))
	{
		CalculateMikkTSpace(m);
	}

	if(!m.Tangents.empty())
	{
		ProcessTangents(m);
	}

	m.ElementType = DetermineElementType(m);
	PackVertexData(m);
}

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
		su32 + // TODO: PRIMITIVE TOPOLOGY PLACEHOLDER
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

	u64 size{ su32 + group.Name.size() + su32 }; // name and number of LODS
	for (const auto& lod : group.LodGroups)
	{
		u64 lodSize{ su32 + lod.Name.size() + su32 }; // name and number of meshes
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
* mesh name
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
	const u8* indexData{ (const u8*)m.Indices.data() };
	Vec<u16> indices{ indexCount };
	if (indexSize == sizeof(u16))
	{
		indices.resize(indexCount);
		for (u32 i{ 0 }; i < indexCount; ++i)
			indices[i] = (u16)m.Indices[i];
		indexData = (const u8*)indices.data();
	}
	blob.WriteBytes(indexData, indexBufferSize);
}

u64
GetEnginePackedGeometrySize(const MeshGroup& group)
{
	constexpr u64 su32{ sizeof(u32) };

	u64 size{ su32 };
	for (const auto& lod : group.LodGroups)
	{
		size += sizeof(f32) + su32 + su32;
		for (const auto& m : lod.Meshes)
		{
			size += su32 + su32 + su32 + su32 + su32;
			assert(m.PositionBuffer.size() % 4 == 0);

			u64 sizes = math::AlignUp<4>(m.PositionBuffer.size());
			u64 sizes2 = math::AlignUp<4>(m.ElementBuffer.size());
			//size += m.PositionBuffer.size();	
			assert(m.ElementBuffer.size() % 4 == 0);
			//size += m.ElementBuffer.size();
			size += sizes;
			size += sizes2;
			const u32 indexSize{ (m.Vertices.size() < (1 << 16)) ? sizeof(u16) : sizeof(u32) };
			size += indexSize * m.Indices.size();
		}
	}
	return size;
}

void
PackMeshDataForEditor(const Mesh& m, util::BlobStreamWriter& writer)
{
	writer.WriteStringWithLength(m.Name);
	writer.Write(m.LodID);

	const u32 vertexCount{ (u32)m.Vertices.size() };
	const u32 indexCount{ (u32)m.Indices.size() };
	const u32 indexSize{ (vertexCount < (1 << 16)) ? sizeof(u16) : sizeof(u32) };
	const u32 elementSize{ GetVertexElementSize(m.ElementType) };
	writer.Write(elementSize);
	writer.Write((u32)m.ElementType);

	writer.Write(vertexCount);

	writer.Write(indexSize);
	writer.Write(indexCount);

	writer.Write(0.f); //TODO: lod thresholds

	writer.Write(3); //TODO: primitive topology

	assert(m.PositionBuffer.size() == sizeof(v3) * vertexCount);
	writer.WriteBytes(m.PositionBuffer.data(), m.PositionBuffer.size());
	assert(m.ElementBuffer.size() == elementSize * vertexCount);
	writer.WriteBytes(m.ElementBuffer.data(), m.ElementBuffer.size());

	const u32 indexBufferSize{ indexSize * indexCount };
	const u8* indexData{ (const u8*)m.Indices.data() };
	Vec<u16> indices{ indexCount };
	if (indexSize == sizeof(u16))
	{
		indices.resize(indexCount);
		for (u32 i{ 0 }; i < indexCount; ++i)
			indices[i] = (u16)m.Indices[i];
		indexData = (const u8*)indices.data();
	}
	writer.WriteBytes(indexData, indexBufferSize);
}

void
PackGeometryDataForEditor(const MeshGroup& group, MeshGroupData& data)
{
	const u64 groupSize{ GetMeshGroupSize(group) };
	u8* buffer{ new u8[groupSize] };
	u32 bufferSize{ (u32)groupSize };

	util::BlobStreamWriter blob{ buffer, groupSize };
	blob.WriteStringWithLength(group.Name);

	blob.Write((u32)group.LodGroups.size());

	for (const auto& lod : group.LodGroups)
	{
		blob.WriteStringWithLength(lod.Name);

		blob.Write((u32)lod.Meshes.size());

		for (const auto& m : lod.Meshes)
		{
			PackMeshDataForEditor(m, blob);
		}
	}

	assert(blob.Offset() == groupSize);
	//TODO: refactor
	std::filesystem::path modelPath{ "Assets/Generated/" + group.Name };
	std::ofstream file{ modelPath, std::ios::out | std::ios::binary };
	if (!file) return;

	file.write(reinterpret_cast<const char*>(buffer), bufferSize);
	file.close();
}

/* the engine expects data to contain :
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
*/
void
PackGeometryForEngine(const MeshGroup& group)
{
	const u64 groupSize{ GetEnginePackedGeometrySize(group) };	
	u8* buffer{ new u8[groupSize] };
	u32 bufferSize{ (u32)groupSize };

	util::BlobStreamWriter blob{ buffer, groupSize };

	blob.Write((u32)group.LodGroups.size());

	for (const auto& lod : group.LodGroups)
	{
		blob.Write(0);	//TODO: lod thresholds
		blob.Write((u32)lod.Meshes.size());

		u8* sizeOfSubmeshesPos{ (u8*)blob.Position() };
		blob.Skip(sizeof(u32)); // reserve space for the size of submeshes
		u8* submeshesStartPos{ (u8*)blob.Position() };
		for (const auto& m : lod.Meshes)
		{
			const u32 vertexCount{ (u32)m.Vertices.size() };
			const u32 indexCount{ (u32)m.Indices.size() };
			const u32 indexSize{ (vertexCount < (1 << 16)) ? sizeof(u16) : sizeof(u32) };
			blob.Write(GetVertexElementSize(m.ElementType));
			blob.Write(vertexCount);
			blob.Write(indexCount);
			blob.Write((u32)m.ElementType);
			blob.Write(3); //TODO: primitive topology

			blob.WriteBytes(m.PositionBuffer.data(), m.PositionBuffer.size());
			blob.WriteBytes(m.ElementBuffer.data(), m.ElementBuffer.size());

			const u32 indexBufferSize{ indexSize * indexCount };
			const u8* indexData{ (const u8*)m.Indices.data() };
			Vec<u16> indices{ indexCount };
			if (indexSize == sizeof(u16))
			{
				indices.resize(indexCount);
				for (u32 i{ 0 }; i < indexCount; ++i)
					indices[i] = (u16)m.Indices[i];
				indexData = (const u8*)indices.data();
			}
			blob.WriteBytes(indexData, indexBufferSize);
		}
		u32 sizeOfSubmeshes{ (u32)(blob.Position() - submeshesStartPos) };
		u8* submeshesEndPos{ (u8*)blob.Position() };
		blob.JumpTo(sizeOfSubmeshesPos);
		blob.Write(sizeOfSubmeshes);
		blob.JumpTo(submeshesEndPos);
	}

	assert(blob.Offset() == groupSize);
	//TODO: refactor
	std::filesystem::path modelPath{ "Assets/Generated/" + group.Name };
	std::ofstream file{ modelPath, std::ios::out | std::ios::binary };
	if (!file) return;

	file.write(reinterpret_cast<const char*>(buffer), bufferSize);
	file.close();
}

void 
CoalesceMeshes(const LodGroup& lod, Mesh& outCombinedMesh)
{
}

void
ProcessMeshGroupData(MeshGroup& group, const GeometryImportSettings& settings)
{
	SplitMeshesByMaterial(group);

	for (auto& lod : group.LodGroups)
	{
		for (auto& m : lod.Meshes)
		{
			ProcessAndPackVertexData(m, settings);
		}
	}
}

/*
* Format:
* [u32] MeshGroup name length
* MeshGroup name
* [u32] number of LODs
* [u32] LOD name length
* LOD name
* [u32] number of meshes
* [] mesh data
*/
//void 
//PackGeometryDataForEditor(const MeshGroup& meshGroup, MeshGroupData& outData)
//{
//	const u64 groupSize{ GetMeshGroupSize(meshGroup) };
//	outData.Buffer = new u8[groupSize];
//	outData.BufferSize = groupSize;
//
//	util::BlobStreamWriter blob{ outData.Buffer, groupSize };
//	blob.WriteStringWithLength(meshGroup.Name);
//	// number of LODs
//	blob.Write((u32)meshGroup.LodGroups.size());
//
//	for (const auto& lod : meshGroup.LodGroups)
//	{
//		blob.WriteStringWithLength(lod.Name);
//		// number of meshes
//		blob.Write((u32)lod.Meshes.size());
//
//		for (const auto& m : lod.Meshes)
//		{
//			PackMeshData(m, blob);
//		}
//	}
//	assert(blob.Offset() == groupSize);
//}

}
