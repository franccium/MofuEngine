#include "PrimitiveMeshGeneration.h"
#include "Utilities/Logger.h"
#include "Graphics/GeometryData.h"
#include <filesystem>

namespace mofu::content {
namespace {

struct Axis
{
	enum : u32
	{
		x = 0,
		y,
		z
	};
};

Mesh 
GeneratePlaneMesh(PrimitiveMeshInfo info, v3 offset = { -0.5f, 0.f, -0.5f }, bool flipWinding = false)
{
	const u32 horizontalSegments{ math::Clamp(info.segments[Axis::x], 1u, 10u) };
	const u32 verticalSegments{ math::Clamp(info.segments[Axis::y], 1u, 10u) };	
	const f32 horizontalStep{ info.size.x / horizontalSegments };
	const f32 verticalStep{ info.size.y / verticalSegments };
	const f32 uStep{ 1.f / horizontalSegments };
	const f32 vStep{ 1.f / verticalSegments };

	Mesh m{};
	u64 positionsCount{ ((u64)horizontalSegments + 1) * ((u64)verticalSegments + 1) };
	m.Positions.resize(positionsCount);
	Vec<v2> uvs{ positionsCount };
	for (u32 y{ 0 }; y < verticalSegments; ++y)
	{
		for (u32 x{ 0 }; x < horizontalSegments; ++x)
		{
			v3 position{ offset };
			position.x += x * horizontalStep;
			position.y += y * verticalStep;

			const u32 index{ y * horizontalSegments + x };
			m.Positions[index] = position;

			uvs[index].x = x * uStep;
			uvs[index].y = 1.f - y * vStep;
		}
	}

	const u32 verticesInRow{ horizontalSegments + 1 };
	for (u32 y{ 0 }; y < verticalSegments; ++y)
	{
		for (u32 x{ 0 }; x < horizontalSegments; ++x)
		{
			const u32 index[4]
			{
				x + y * verticesInRow,
				x + (y + 1) * verticesInRow,
				(x + 1) + y * verticesInRow,
				(x + 1) + (y + 1) * verticesInRow
			};

			m.RawIndices.emplace_back(index[0]);
			m.RawIndices.emplace_back(index[flipWinding ? 2 : 1]);
			m.RawIndices.emplace_back(index[flipWinding ? 1 : 2]);

			m.RawIndices.emplace_back(index[2]);
			m.RawIndices.emplace_back(index[flipWinding ? 3 : 1]);
			m.RawIndices.emplace_back(index[flipWinding ? 1 : 3]);
		}
	}

	const u32 numIndices{ 3 * 2 * horizontalSegments * verticalSegments };
	assert(m.RawIndices.size() == numIndices);

	m.UvSets.resize(1);
	m.UvSets[0].resize(numIndices);
	for (u32 i{ 0 }; i < numIndices; ++i)
	{
		m.UvSets[0][i] = uvs[m.RawIndices[i]];
	}

	log::Info("Generated a plane mesh with %u vertices and %u indices", m.Positions.size(), m.RawIndices.size());

	return m;
}

void 
CreatePlane(MeshGroup* meshGroup, PrimitiveMeshInfo info)
{
	LodGroup lodGroup{};
	lodGroup.Name = "Plane";
	lodGroup.Meshes.emplace_back(std::move(GeneratePlaneMesh(info)));
	meshGroup->LodGroups.emplace_back(std::move(lodGroup));
}

void CreateCube([[maybe_unused]] MeshGroup* meshGroup, [[maybe_unused]] PrimitiveMeshInfo info) {}
void CreateUvSphere([[maybe_unused]] MeshGroup* meshGroup, [[maybe_unused]] PrimitiveMeshInfo info) {}
void CreateIcosphere([[maybe_unused]] MeshGroup* meshGroup, [[maybe_unused]] PrimitiveMeshInfo info) {}
void CreateCylinder([[maybe_unused]] MeshGroup* meshGroup, [[maybe_unused]] PrimitiveMeshInfo info) {}
void CreateCapsule([[maybe_unused]] MeshGroup* meshGroup, [[maybe_unused]] PrimitiveMeshInfo info) {}

using MeshCreator = void(*)(MeshGroup* meshGroup, PrimitiveMeshInfo info);

constexpr MeshCreator meshCreators[PrimitiveMeshType::Count]
{
	CreatePlane,
	CreateCube,
	CreateUvSphere,
	CreateIcosphere,
	CreateCylinder,
	CreateCapsule,
};
static_assert(_countof(meshCreators) == PrimitiveMeshType::Count, "Mesh creator array size mismatch");



}

void 
GeneratePrimitiveMesh(PrimitiveMeshInfo info, MeshGroupData& outData)
{
	log::Info("Generating mesh: %u", info.type);
	MeshGroup meshGroup{};
	meshCreators[info.type](&meshGroup, info);

	ProcessMeshGroupData(meshGroup, outData.ImportSettings);
	PackGeometryData(meshGroup, outData);
}

}