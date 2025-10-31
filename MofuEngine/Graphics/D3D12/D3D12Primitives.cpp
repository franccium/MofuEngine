#include "D3D12Primitives.h"

namespace mofu::graphics::d3d12::primitives {
namespace {
util::FreeList<Primitive> _primitives[2]{};
} // anonymous namespace

u32
AddPrimitive(Primitive primitive)
{
    u32 idx{ _primitives[0].add(primitive) };
    return idx;
}

void
RemovePrimitive(u32 primitiveIdx)
{
    assert(primitiveIdx < _primitives[0].size());
    _primitives[0].remove(primitiveIdx);
}

void 
CreateVertexBuffer(u32 primitiveIdx, u32 vertexSize, u32 vertexCount, void* data)
{
    assert(primitiveIdx < _primitives[0].size());
    ReleaseVertexBuffer(primitiveIdx);
    Primitive& primitive{ _primitives[0][primitiveIdx] };
    primitive.VertexCount = vertexCount;
    primitive.NumVertexToDraw = vertexCount;
    primitive.VertexSize = vertexSize;
    u32 totalSize{ vertexSize * vertexCount };
    primitive.VertexBuffer = d3dx::CreateResourceBuffer(data, totalSize, true);
}

void 
LockVertexBuffer(u32 primitiveIdx)
{
}

void 
ReleaseVertexBuffer(u32 primitiveIdx)
{
    assert(primitiveIdx < _primitives[0].size());
    Primitive& primitive{ _primitives[0][primitiveIdx] };
    if (!primitive.VertexBuffer) return;
    core::Release(primitive.VertexBuffer);
    primitive.VertexCount = 0;
    primitive.NumVertexToDraw = 0;
    primitive.VertexSize = 0;
    _primitives[0].remove(primitiveIdx);
}

void
DrawPrimitives()
{
    DXGraphicsCommandList* const cmdList{ core::GraphicsCommandList() };
    for (u32 i{ 0 }; i < _primitives[0].size(); ++i)
    {
        Primitive& primitive{ _primitives[0][i] };
        if (primitive.VertexBuffer != nullptr)
        {
            cmdList->IASetPrimitiveTopology((D3D12_PRIMITIVE_TOPOLOGY)primitive.PrimitiveTopology);
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