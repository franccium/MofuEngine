#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::primitives {
//TODO: bindless
struct Primitive
{
	enum Topology : u8
	{
		Line = D3D_PRIMITIVE_TOPOLOGY_LINELIST,
		Triangle = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		Count = 2
	};

	Topology PrimitiveTopology;
	DXResource* VertexBuffer{ nullptr };
	u32 VertexCount{ 0 };
	u32 NumVertexToDraw{ 0 };
	u32 VertexSize{ 0 };
};

u32 AddPrimitive(Primitive primitive);
void RemovePrimitive(u32 primitiveIdx);
void CreateVertexBuffer(u32 primitiveIdx, u32 vertexSize, u32 vertexCount, void* data);
void LockVertexBuffer(u32 primitiveIdx);
void ReleaseVertexBuffer(u32 primitiveIdx);

void DrawPrimitives();
}