#pragma once
#include "D3D12CommonHeaders.h"
#include <Jolt/Jolt.h>
#include <Jolt/Core/Reference.h>

namespace mofu::graphics::d3d12::primitives {
//TODO: bindless
struct Primitive : public JPH::RefTarget<Primitive>, public JPH::RefTargetVirtual
{
	enum Topology : u8
	{
		Line = 0,
		Triangle = 1,
		Count
	};

	Topology PrimitiveTopology;
	DXResource* VertexBuffer{ nullptr };
	DXResource* IndexBuffer{ nullptr };
	u32 VertexCount{ 0 };
	u32 NumVertexToDraw{ 0 };
	u32 VertexSize{ 0 };

	u32 IndexCount{ 0 };
	u32 NumIndexToDraw{ 0 };

	virtual void AddRef() override { JPH::RefTarget<Primitive>::AddRef(); }
	virtual void Release() override { JPH::RefTarget<Primitive>::Release(); }
};

struct D3D12Instance : public JPH::RefTarget<D3D12Instance>
{
	//u32 VertexIdx{ U32_INVALID_ID };
	//u32 IndexIdx{ U32_INVALID_ID };
	DXResource* InstanceBuffer{ nullptr };
	u32 InstanceBufferSize{ 0 };
	u32 InstanceSize{ 0 };

	u32 VertexCount{ 0 };
	u32 NumVertexToDraw{ 0 };
	u32 VertexSize{ 0 };

	u32 IndexCount{ 0 };
	u32 NumIndexToDraw{ 0 };
};

u32 AddPrimitive();
void RemovePrimitive(u32 primitiveIdx);
void CreateVertexBuffer(u32 primitiveIdx, u32 vertexSize, u32 vertexCount, void* data);
void CreateIndexBuffer(u32 primitiveIdx, u32 indexCount, void* data);
void* const MapVertexBuffer(u32 primitiveIdx);
void* const MapIndexBuffer(u32 primitiveIdx);
void UnmapVertexBuffer(u32 primitiveIdx);
void UnmapIndexBuffer(u32 primitiveIdx);
void ReleaseVertexBuffer(u32 primitiveIdx);
void ReleaseIndexBuffer(u32 primitiveIdx);
Primitive* GetPrimitive(u32 primitiveIdx);

D3D12Instance* AddInstance();
D3D12Instance* const GetInstanceBuffer();
void* const MapInstanceBuffer(const D3D12Instance* instance);
void CreateInstanceBuffer(u32 instanceCount, u32 instanceSize, void* data);
void RemoveInstance();
}