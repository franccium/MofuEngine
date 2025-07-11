#pragma once
#include "CommonHeaders.h"
#include <filesystem>

namespace mofu::content {
struct ElementType
{
	enum type : u32
	{
		PositionOnly = 0x00,
		StaticNormal = 0x01,
		StaticNormalTexture = 0x03, // if the mesh uses texture mapping, it must have normals and tangets, as well as uv coords; 2nd bit - tangent and uv info, meshes without normals not supported
		StaticColor = 0x04,
		Skeletal = 0x08,
		SkeletalColor = Skeletal | StaticColor,
		SkeletalNormal = Skeletal | StaticNormal,
		SkeletalNormalColor = SkeletalNormal | StaticColor,
		SkeletalNormalTexture = Skeletal | StaticNormalTexture,
		SkeletalNormalTextureColor = SkeletalNormalTexture | StaticColor
	};
};

struct PositionOnly {};

struct StaticColor
{
	u8 Color[3];
	u8 _pad;
};

struct StaticNormal
{
	u8 Color[3];
	u8 TNSign; // bit 0: tangent handedness, bit 1: tangent.z sign, bit 2: normal.z sign (0 for -1, 1 for +1)
	u16 Normal[2]; // normal packed as xy, reconstruct with normal.z sign
};

struct StaticNormalTexture
{
	u8 Color[3];
	u8 TNSign; // bit 0: tangent handedness, bit 1: tangent.z sign, bit 2: normal.z sign (0 for -1, 1 for +1)
	v2 UV;
	u16 Normal[2]; // normal packed as xy, reconstruct with normal.z sign
	u16 tangent[2];
};

struct Skeletal
{
	u8 JointWeights[3]; // normalized joint weights for up to 4 joints
	u8 _pad;
	u16 JointIndices[4];
};

struct SkeletalColor
{
	u8 JointWeights[3]; // normalized joint weights for up to 4 joints
	u8 _pad;
	u16 JointIndices[4];
	u8 Color[3];
	u8 _pad2;
};

struct SkeletalNormal
{
	u8 JointWeights[3]; // normalized joint weights for up to 4 joints
	u8 TNSign; // bit 0: tangent handedness, bit 1: tangent.z sign, bit 2: normal.z sign (0 for -1, 1 for +1)
	u16 JointIndices[4];
	u16 Normal[2]; // normal packed as xy, reconstruct with normal.z sign
};

struct SkeletalNormalColor
{
	u8 JointWeights[3]; // normalized joint weights for up to 4 joints
	u8 TNSign; // bit 0: tangent handedness, bit 1: tangent.z sign, bit 2: normal.z sign (0 for -1, 1 for +1)
	u16 JointIndices[4];
	u16 Normal[2]; // normal packed as xy, reconstruct with normal.z sign
	u8 Color[3];
	u8 _pad;
};

struct SkeletalNormalTexture
{
	u8 JointWeights[3]; // normalized joint weights for up to 4 joints
	u8 TNSign; // bit 0: tangent handedness, bit 1: tangent.z sign, bit 2: normal.z sign (0 for -1, 1 for +1)
	u16 JointIndices[4];
	u16 Normal[2]; // normal packed as xy, reconstruct with normal.z sign
	u16 Tangent[2];
	v2 UV;
};

struct SkeletalNormalTextureColor
{
	u8 JointWeights[3]; // normalized joint weights for up to 4 joints
	u8 TNSign; // bit 0: tangent handedness, bit 1: tangent.z sign, bit 2: normal.z sign (0 for -1, 1 for +1)
	u16 JointIndices[4];
	u16 Normal[2]; // normal packed as xy, reconstruct with normal.z sign
	u16 Tangent[2];
	v2 UV;
	u8 Color[3];
	u8 _pad;
};

constexpr u32 ElementSizes[ElementType::SkeletalNormalTextureColor + 1]
{
	sizeof(PositionOnly),
	sizeof(StaticNormal),
	0,
	sizeof(StaticNormalTexture),
	sizeof(StaticColor),
	0,0,0,
	sizeof(Skeletal),
	sizeof(SkeletalColor),
	sizeof(SkeletalNormal),
	sizeof(SkeletalNormalColor),
	sizeof(SkeletalNormalTexture),
	sizeof(SkeletalNormalTextureColor)
};

constexpr u32 GetVertexElementSize(ElementType::type type)
{
	assert(type <= ElementType::SkeletalNormalTextureColor);
	return ElementSizes[type];
}

struct Vertex
{
	v4 Tangent{};
	v4 JointWeights{};
	u32v4 JointIndices{ U32_INVALID_ID, U32_INVALID_ID, U32_INVALID_ID, U32_INVALID_ID };
	v3 Position{};
	v3 Normal{};
	v2 UV{};
	u8 Red{}, Green{}, Blue{};
	u8 _pad;
};

struct Mesh
{
	// initial data
	Vec<v3> Positions;
	Vec<v3> Normals;
	Vec<v4> Tangents;
	Vec<v3> Colors;
	Vec<Vec<v2>> UvSets;
	Vec<u32> MaterialIndices;
	Vec<u32> MaterialUsed;
	Vec<u32> RawIndices;

	Vec<Vertex> Vertices;
	Vec<u32> Indices;

	std::string Name;
	ElementType::type ElementType;
	Vec<u8> PositionBuffer;
	Vec<u8> ElementBuffer;
	f32 LodThreshold;
	u32 LodID{ U32_INVALID_ID };
};

struct LodGroup
{
	std::string Name;
	Vec<Mesh> Meshes;
};

struct MeshGroup
{
	std::string Name;
	Vec<LodGroup> LodGroups;
};

struct GeometryImportSettings
{
	f32 SmoothingAngle;
	bool CalculateNormals;
	bool CalculateTangents;
	bool ReverseHandedness;
	bool ImportEmbeddedTextures;
	bool ImportAnimations;
	bool MergeMeshes;
};

struct MeshGroupData
{
	u8* Buffer;
	u32 BufferSize;
	GeometryImportSettings ImportSettings;
};

void PackGeometryDataForEditor(const MeshGroup& meshGroup, MeshGroupData& data, std::filesystem::path targetPath);
void ProcessMeshGroupData(MeshGroup& group, const GeometryImportSettings& settings);

void PackGeometryForEngine(const MeshGroup& group, std::filesystem::path targetPath);

void MergeMeshes(const LodGroup& lod, Mesh& outCombinedMesh);
}