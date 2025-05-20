#pragma once
#include "CommonHeaders.h"

/*
* generates primitive meshes and stores them in a binary format that's used by the engine
*/

namespace mofu::content {
struct MeshGroupData;

enum PrimitiveMeshType : u32
{
	Plane,
	Cube,
	UvSphere,
	IcoSphere,
	Cylinder,
	Capsule,

	Count
};

struct PrimitiveMeshInfo
{
	PrimitiveMeshType type;
	u32 segments[3]{ 1, 1, 1 }; // segments for each axis		
	v3 size{ 1.0f, 1.0f, 1.0f };
};

void GeneratePrimitiveMesh(PrimitiveMeshInfo info, MeshGroupData& outData);
}