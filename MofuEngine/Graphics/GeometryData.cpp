#include "GeometryData.h"
#include "Utilities/IOStream.h"
#include "External/MikkTSpace/mikktspace.h"
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

	m.Vertices.clear(); // TODO: ??

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


s32
MikkGetNumFaces(const SMikkTSpaceContext* context)
{
	const Mesh& m{ *(Mesh*)(context->m_pUserData) };
	return (s32)m.Indices.size() / 3; // we only use triangle meshes
}

s32
MikkGetNumVerticesOfFace([[maybe_unused]] const SMikkTSpaceContext* context, [[maybe_unused]] s32 faceIdx)
{
	// we only use triangle meshes
	return 3;
}

void
MikkGetPosition(const SMikkTSpaceContext* context, f32 position[3], s32 faceIdx, s32 vertexIdx)
{
	const Mesh& m{ *(Mesh*)(context->m_pUserData) };
	const u32 index{ m.Indices[faceIdx * 3 + vertexIdx] };
	const v3& p{ m.Vertices[index].Position };
	position[0] = p.x;
	position[1] = p.y;
	position[2] = p.z;
}

void
MikkGetNormal(const SMikkTSpaceContext* context, f32 normal[3], s32 faceIdx, s32 vertexIdx)
{
	const Mesh& m{ *(Mesh*)(context->m_pUserData) };
	const u32 index{ m.Indices[faceIdx * 3 + vertexIdx] };
	const v3& n{ m.Vertices[index].Normal };
	normal[0] = n.x;
	normal[1] = n.y;
	normal[2] = n.z;
}

void
MikkGetTexCoord(const SMikkTSpaceContext* context, f32 tex_coords[2], s32 faceIdx, s32 vertexIdx)
{
	const Mesh& m{ *(Mesh*)(context->m_pUserData) };
	const u32 index{ m.Indices[faceIdx * 3 + vertexIdx] };
	const v2& uv{ m.Vertices[index].UV };
	tex_coords[0] = uv.x;
	tex_coords[1] = uv.y;
}

void
MikkSetTSpaceBasic(const SMikkTSpaceContext* context, const f32 tangent[3], f32 sign, s32 faceIdx, s32 vertexIdx)
{
	Mesh& m{ *(Mesh*)(context->m_pUserData) };
	const u32 index{ m.Indices[faceIdx * 3 + vertexIdx] };
	v4& t{ m.Vertices[index].Tangent };
	t.x = tangent[0];
	t.y = tangent[1];
	t.z = tangent[2];
	t.w = sign;
}

void
CalculateMikkTSpace(Mesh& m)
{
	m.Tangents.clear();

	SMikkTSpaceInterface mikkInterface{};
	mikkInterface.m_getNumFaces = MikkGetNumFaces;
	mikkInterface.m_getNumVerticesOfFace = MikkGetNumVerticesOfFace;
	mikkInterface.m_getPosition = MikkGetPosition;
	mikkInterface.m_getNormal = MikkGetNormal;
	mikkInterface.m_getTexCoord = MikkGetTexCoord;
	mikkInterface.m_setTSpaceBasic = MikkSetTSpaceBasic;
	mikkInterface.m_setTSpace = nullptr;

	SMikkTSpaceContext mikkContext{};
	mikkContext.m_pInterface = &mikkInterface;
	mikkContext.m_pUserData = (void*)&m;

	genTangSpaceDefault(&mikkContext);
}

void
CalculateTangents(Mesh& m)
{
	m.Tangents.clear();

	const u32 numIndices{ (u32)m.RawIndices.size() };
	Vec<xmm> tangents(numIndices, XMVectorZero());
	Vec<xmm> bitangents(numIndices, XMVectorZero());
	Vec<xmm> positions(numIndices);

	for (u32 i{ 0 }; i < numIndices; ++i)
	{
		positions[i] = XMLoadFloat3(&m.Vertices[m.Indices[i]].Position);
	}

	for (u32 i{ 0 }; i < numIndices; i += 3)
	{
		const u32 i1{ i + 1 };
		const u32 i2{ i + 2 };

		const xmm p0{ positions[i] };
		const xmm p1{ positions[i1] };
		const xmm p2{ positions[i2] };

		const v2& uv0{ m.Vertices[m.Indices[i]].UV };
		const v2& uv1{ m.Vertices[m.Indices[i1]].UV };
		const v2& uv2{ m.Vertices[m.Indices[i2]].UV };

		const v2 duv1{ uv1.x - uv0.x, uv1.y - uv0.y };
		const v2 duv2{ uv2.x - uv0.x, uv2.y - uv0.y };

		const XMVECTOR dp1{ p1 - p0 };
		const XMVECTOR dp2{ p2 - p0 };

		f32 det{ duv1.x * duv2.y - duv1.y * duv2.x };
		if (abs(det) < math::EPSILON) det = math::EPSILON;

		const f32 invDet{ 1.f / det };
		const XMVECTOR t{ (dp1 * duv2.y - dp2 * duv1.y) * invDet };
		const XMVECTOR b{ (dp2 * duv1.x - dp1 * duv2.x) * invDet };

		tangents[i] += t;
		tangents[i1] += t;
		tangents[i2] += t;
		bitangents[i] += b;
		bitangents[i1] += b;
		bitangents[i2] += b;
	}

	// orthonoramlize and calculate handedness
	for (u32 i{ 0 }; i < numIndices; i += 3)
	{
		const XMVECTOR& t{ tangents[i] };
		const XMVECTOR& b{ bitangents[i] };

		const XMVECTOR& n{ XMLoadFloat3(&m.Vertices[m.Indices[i]].Normal) };
		v3 tangent;
		XMStoreFloat3(&tangent, XMVector3Normalize(t - n * XMVectorDivide(n, t)));

		f32 handedness;
		XMStoreFloat(&handedness, XMVector3Dot(XMVector3Cross(t, b), n));
		handedness = handedness > 0 ? 1.f : -1.f;

		m.Vertices[m.Indices[i]].Tangent = { tangent.x, tangent.y, tangent.z, handedness };
	}
}

void
ProcessTangents(Mesh& m)
{
	if (m.Tangents.size() != m.RawIndices.size()) return;

	Vec<Vertex> oldVertices(m.Vertices.size());
	oldVertices.swap(m.Vertices);
	Vec<u32> oldIndices(m.Indices.size());
	oldIndices.swap(m.Indices);

	const u32 numVertices{ (u32)oldVertices.size() };
	const u32 numIndices{ (u32)oldIndices.size() };
	assert(numIndices && numVertices);

	Vec<Vec<u32>> vIdRef(numIndices);
	for (u32 i{ 0 }; i < numIndices; ++i)
		vIdRef[oldIndices[i]].emplace_back(i);

	XMVECTOR xmEpsilon{ XMVectorReplicate(math::EPSILON) };

	for (u32 i{ 0 }; i < numVertices; ++i)
	{
		Vec<u32>& refs{ vIdRef[i] };
		u32 numRefs{ (u32)refs.size() };
		for (u32 j{ 0 }; j < numRefs; ++j)
		{
			const v4& tj{ m.Tangents[refs[j]] };
			Vertex& v{ oldVertices[oldIndices[refs[j]]] };
			v.Tangent = tj;
			m.Indices[refs[j]] = (u32)m.Vertices.size();
			m.Vertices.emplace_back(v);

			XMVECTOR xmTj{ XMLoadFloat4(&tj) };

			for (u32 k{ j + 1 }; k < numRefs; ++k)
			{
				XMVECTOR xmTangent{ XMLoadFloat4(&m.Tangents[refs[k]]) };
				if (XMVector4NearEqual(xmTj, xmTangent, xmEpsilon))
				{
					m.Indices[refs[k]] = m.Indices[refs[j]];
					refs.erase(refs.begin() + k);
					--numRefs;
					--k;
				}
			}
		}
	}
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
			elementBuffer[i] = { {v.Red, v.Green, v.Blue}, tnSigns[i], v.UV, { normals[i].x, normals[i].y }, {tangents[i].x, tangents[i].y} };
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

	//TODO: ??
	//ProcessNormals(m, settings.SmoothingAngle);
	m.Indices = m.RawIndices; //TODO: for now not processing any indices just keep them how ufbx imported them in

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
		su32 + // number of Indices
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
			//size += 4096;
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
PackGeometryDataForEditor(const MeshGroup& group, MeshGroupData& data, std::filesystem::path targetPath)
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
	std::filesystem::path modelPath{ targetPath.replace_extension(".emodel") };
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
PackGeometryForEngine(const MeshGroup& group, std::filesystem::path targetPath)
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

			//u8 fill[2048];
			//memset(fill, 0xFF, 2048);
			//blob.WriteBytes(fill, 2048);
			u64 sizes = math::AlignUp<4>(m.PositionBuffer.size());
			u64 sizes2 = math::AlignUp<4>(m.ElementBuffer.size());
			assert(sizes == m.PositionBuffer.size());
			assert(sizes2 == m.ElementBuffer.size());

			blob.WriteBytes(m.PositionBuffer.data(), sizes);
			//memset(fill, 0xEE, 2048);
			//blob.WriteBytes(fill, 2048);

			auto rawPtr = m.ElementBuffer.data();
			const StaticNormalTexture* debugPtr = reinterpret_cast<const StaticNormalTexture*>(rawPtr);

			mofu::content::StaticNormalTexture s0 = debugPtr[0];
			mofu::content::StaticNormalTexture s1 = debugPtr[1];
			mofu::content::StaticNormalTexture s2 = debugPtr[2];

			blob.WriteBytes(m.ElementBuffer.data(), sizes2);

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
	std::filesystem::path modelPath{ targetPath.replace_extension(".model") };
	std::ofstream file{ modelPath, std::ios::out | std::ios::binary };
	if (!file) return;

	file.write(reinterpret_cast<const char*>(buffer), bufferSize);
	file.close();
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
