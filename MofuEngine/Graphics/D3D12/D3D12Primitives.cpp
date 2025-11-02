#include "D3D12Primitives.h"

namespace mofu::graphics::d3d12::primitives {
namespace {
//NOTE: Jolt expects stable pointers as DebugRenderer::Batch for its default shapes
//NOTE: find out if i ever run into this problem with my own shapes
//TODO: maybe fork jolt
constexpr u32 TRIANGLE_PRIMITIVE_BATCHES_COUNT{ 256 }; // jolt needs 26 or sth default
util::FreeList<Primitive> _primitives{ TRIANGLE_PRIMITIVE_BATCHES_COUNT };
constexpr u32 JOLT_INDEX_SIZE{ sizeof(u32) };
constexpr Primitive::Topology INVALID_TOPOLOGY_MARK{ 9 };

D3D12Instance _instances[FRAME_BUFFER_COUNT]{};
} // anonymous namespace

u32
AddPrimitive()
{
    u32 idx{ _primitives.add() };
    assert(idx < TRIANGLE_PRIMITIVE_BATCHES_COUNT);
    _primitives[idx].PrimitiveTopology = Primitive::Topology::Triangle;
    return idx;
}

void
RemovePrimitive(u32 primitiveIdx)
{
    assert(primitiveIdx < _primitives.capacity());
    ReleaseVertexBuffer(primitiveIdx);
    ReleaseIndexBuffer(primitiveIdx);
    _primitives.remove(primitiveIdx);
    _primitives[primitiveIdx].PrimitiveTopology = INVALID_TOPOLOGY_MARK;
}

void 
CreateVertexBuffer(u32 primitiveIdx, u32 vertexSize, u32 vertexCount, void* data)
{
    assert(primitiveIdx < _primitives.capacity());
    ReleaseVertexBuffer(primitiveIdx);
    Primitive& primitive{ _primitives[primitiveIdx] };
    primitive.VertexCount = vertexCount;
    primitive.NumVertexToDraw = vertexCount;
    primitive.VertexSize = vertexSize;
    u32 totalSize{ vertexSize * vertexCount };
    primitive.VertexBuffer = d3dx::CreateResourceBuffer(data, totalSize, true);
}

void
CreateIndexBuffer(u32 primitiveIdx, u32 indexCount, void* data)
{
    assert(primitiveIdx < _primitives.capacity());
    ReleaseIndexBuffer(primitiveIdx);
    Primitive& primitive{ _primitives[primitiveIdx] };
    primitive.IndexCount = indexCount;
    primitive.NumIndexToDraw = indexCount;
    u32 totalSize{ indexCount * JOLT_INDEX_SIZE };
    primitive.IndexBuffer = d3dx::CreateResourceBuffer(data, totalSize, true);
}

void* const
MapVertexBuffer(u32 primitiveIdx)
{
    assert(primitiveIdx < _primitives.capacity());
    const D3D12_RANGE range{};
    void* cpuAddress{ nullptr };
    DXCall(_primitives[primitiveIdx].VertexBuffer->Map(0, &range, reinterpret_cast<void**>(&cpuAddress)));
    assert(cpuAddress);
    return cpuAddress;
}

void* const
MapIndexBuffer(u32 primitiveIdx)
{
    assert(primitiveIdx < _primitives.capacity());
    const D3D12_RANGE range{};
    void* cpuAddress{ nullptr };
    DXCall(_primitives[primitiveIdx].IndexBuffer->Map(0, &range, reinterpret_cast<void**>(&cpuAddress)));
    assert(cpuAddress);
    return cpuAddress;
}

void 
UnmapVertexBuffer(u32 primitiveIdx)
{
    assert(primitiveIdx < _primitives.capacity());
    _primitives[primitiveIdx].VertexBuffer->Unmap(0, nullptr);
}

void
UnmapIndexBuffer(u32 primitiveIdx)
{
    assert(primitiveIdx < _primitives.capacity());
    _primitives[primitiveIdx].IndexBuffer->Unmap(0, nullptr);
}

void 
ReleaseVertexBuffer(u32 primitiveIdx)
{
    assert(primitiveIdx < _primitives.capacity());
    Primitive& primitive{ _primitives[primitiveIdx] };
    if (!primitive.VertexBuffer) return;
    core::Release(primitive.VertexBuffer);
    primitive.VertexCount = 0;
    primitive.NumVertexToDraw = 0;
    primitive.VertexSize = 0;
}

void
ReleaseIndexBuffer(u32 primitiveIdx)
{
    assert(primitiveIdx < _primitives.capacity());
    Primitive& primitive{ _primitives[primitiveIdx] };
    if (!primitive.IndexBuffer) return;

    core::Release(primitive.IndexBuffer);
    primitive.IndexCount = 0;
    primitive.NumIndexToDraw = 0;
}

Primitive* GetPrimitive(u32 primitiveIdx)
{
    assert(primitiveIdx < _primitives.capacity());
    return &_primitives[primitiveIdx];
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

}