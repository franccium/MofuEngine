#include "D3D12Primitives.h"

namespace mofu::graphics::d3d12::primitives {
namespace {
//NOTE: Jolt expects stable pointers as DebugRenderer::Batch for its default shapes
//NOTE: find out if i ever run into this problem with my own shapes
//TODO: maybe fork jolt
constexpr u32 TRIANGLE_PRIMITIVE_BATCHES_COUNT{ 256 }; // jolt needs 26 or sth default
util::FreeList<Primitive> _primitives[2]{ {0}, {TRIANGLE_PRIMITIVE_BATCHES_COUNT} };
constexpr u32 JOLT_INDEX_SIZE{ sizeof(u32) };
constexpr Primitive::Topology INVALID_TOPOLOGY_MARK{ 9 };

D3D12Instance _instances[FRAME_BUFFER_COUNT]{};
} // anonymous namespace

u32
AddPrimitive(Primitive::Topology topology)
{
    u32 idx{ _primitives[topology].add() };
    _primitives[topology][idx].PrimitiveTopology = topology;
    return idx;
}

void
RemovePrimitive(Primitive::Topology topology, u32 primitiveIdx)
{
    assert(primitiveIdx < _primitives[topology].capacity());
    _primitives[topology].remove(primitiveIdx);
    _primitives[topology][primitiveIdx].PrimitiveTopology = INVALID_TOPOLOGY_MARK;
}

void 
CreateVertexBuffer(Primitive::Topology topology, u32 primitiveIdx, u32 vertexSize, u32 vertexCount, void* data)
{
    assert(primitiveIdx < _primitives[topology].capacity());
    ReleaseVertexBuffer(topology, primitiveIdx);
    Primitive& primitive{ _primitives[topology][primitiveIdx] };
    primitive.VertexCount = vertexCount;
    primitive.NumVertexToDraw = vertexCount;
    primitive.VertexSize = vertexSize;
    u32 totalSize{ vertexSize * vertexCount };
    primitive.VertexBuffer = d3dx::CreateResourceBuffer(data, totalSize, true);
}

void
CreateIndexBuffer(Primitive::Topology topology, u32 primitiveIdx, u32 indexCount, void* data)
{
    assert(primitiveIdx < _primitives[topology].capacity());
    ReleaseIndexBuffer(topology, primitiveIdx);
    Primitive& primitive{ _primitives[topology][primitiveIdx] };
    primitive.IndexCount = indexCount;
    primitive.NumIndexToDraw = indexCount;
    u32 totalSize{ indexCount * JOLT_INDEX_SIZE };
    primitive.IndexBuffer = d3dx::CreateResourceBuffer(data, totalSize, true);
}

void 
LockVertexBuffer(Primitive::Topology topology, u32 primitiveIdx)
{
}

void 
ReleaseVertexBuffer(Primitive::Topology topology, u32 primitiveIdx)
{
    assert(primitiveIdx < _primitives[topology].capacity());
    Primitive& primitive{ _primitives[topology][primitiveIdx] };
    if (!primitive.VertexBuffer) return;
    core::Release(primitive.VertexBuffer);
    primitive.VertexCount = 0;
    primitive.NumVertexToDraw = 0;
    primitive.VertexSize = 0;
}

void
ReleaseIndexBuffer(Primitive::Topology topology, u32 primitiveIdx)
{
    assert(primitiveIdx < _primitives[topology].capacity());
    Primitive& primitive{ _primitives[topology][primitiveIdx] };
    if (!primitive.IndexBuffer) return;

    core::Release(primitive.IndexBuffer);
    primitive.IndexCount = 0;
    primitive.NumIndexToDraw = 0;
}

Primitive* GetPrimitive(Primitive::Topology topology, u32 primitiveIdx)
{
    assert(primitiveIdx < _primitives[topology].capacity());
    return &_primitives[topology][primitiveIdx];
}

D3D12Instance* const
GetInstanceBuffer()
{
    return &_instances[core::CurrentFrameIndex()];
}

void 
CreateInstanceBuffer(u32 instanceCount, u32 instanceSize, void* data)
{
    D3D12Instance& instance{ _instances[core::CurrentFrameIndex()] };
    const u32 newTotalSize{ instanceCount * instanceSize };
    if (!instance.InstanceBuffer || instance.InstanceBufferSize < newTotalSize)
    {
        core::Release(instance.InstanceBuffer);
        instance.InstanceBuffer = nullptr;
        instance.InstanceBufferSize = 0;
        instance.InstanceSize = 0;

        instance.InstanceBuffer = d3dx::CreateResourceBuffer(data, newTotalSize, true);
        instance.InstanceBufferSize = newTotalSize;
    }
    instance.InstanceSize = instanceSize;
}

void* const
MapInstanceBuffer(const D3D12Instance* instance)
{
    const D3D12_RANGE range{};
    void* cpuAddress{ nullptr };
    DXCall(instance->InstanceBuffer->Map(0, &range, reinterpret_cast<void**>(&cpuAddress)));
    assert(cpuAddress);
    return cpuAddress;
}

void
DrawPrimitives(Primitive::Topology topology)
{
    DXGraphicsCommandList* const cmdList{ core::GraphicsCommandList() };
    for (u32 i{ 0 }; i < _primitives[topology].capacity(); ++i)
    {
        if (_primitives[topology][i].PrimitiveTopology == INVALID_TOPOLOGY_MARK) continue; // TODO:
        Primitive& primitive{ _primitives[topology][i] };
        if (primitive.VertexBuffer != nullptr)
        {
            cmdList->IASetPrimitiveTopology(primitive.PrimitiveTopology == Primitive::Topology::Line ? D3D_PRIMITIVE_TOPOLOGY_LINELIST : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            D3D12_VERTEX_BUFFER_VIEW vb{};
            vb.BufferLocation = primitive.VertexBuffer->GetGPUVirtualAddress();
            vb.SizeInBytes = primitive.VertexSize * primitive.NumVertexToDraw;
            vb.StrideInBytes = primitive.VertexSize;
            cmdList->IASetVertexBuffers(0, 1, &vb);
            cmdList->DrawInstanced(primitive.NumVertexToDraw, 1, 0, 0);
        }
    }
}
}