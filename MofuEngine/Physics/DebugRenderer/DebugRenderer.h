#pragma once
#include "Physics/JoltCommon.h"

#include <Jolt/Core/Mutex.h>
#include <Jolt/Core/UnorderedMap.h>
#include <Jolt/Renderer/DebugRenderer.h>
#include "Graphics/D3D12/D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::debug {
class DebugRenderer final : public JPH::DebugRenderer
{
public:
    JPH_OVERRIDE_NEW_DELETE;
    DebugRenderer();

    void Draw(const D3D12FrameInfo& frameInfo);
    void Clear();
    void CreatePSOs();

    void DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color) override;

    void DrawTriangle(JPH::RVec3Arg v1, JPH::RVec3Arg v2, JPH::RVec3Arg v3, JPH::ColorArg color, ECastShadow castShadow) override;

    void DrawText3D(JPH::RVec3Arg position, const std::string_view& string, JPH::ColorArg color, f32 height) override;

    void DrawGeometry(JPH::RMat44Arg modelMatrix, const JPH::AABox& worldSpaceBounds, f32 LODScaleSq, JPH::ColorArg modelColor, const GeometryRef& geometry, ECullMode cullMode, ECastShadow castShadow, EDrawMode drawMode) override;

    Batch CreateTriangleBatch(const Triangle* triangles, s32 triangleCount) override;
    Batch CreateTriangleBatch(const Vertex* vertices, s32 vertexCount, const u32* indices, s32 indexCount) override;

private:
    struct Line
    {
        JPH::Float3 From;
        JPH::Color FromColor;
        JPH::Float3 To;
        JPH::Color ToColor;
    };
    struct Instance
    {
        JPH::Mat44 TRS;
        JPH::Mat44 TRSInvTrans;
        JPH::Color Color;
        JPH::AABox WorldSpaceBounds;
        f32 LODScaleSq;
    };
    struct InstanceBatch
    {
        Array<Instance> Instances;
        Array<s32> GeometryStartIndex;
        Array<s32> LightStartIndex;
    };
    struct Text
    {
        v3 Position;
        f32 Height;
        JPH::Color Color;
        JPH::String String;
    };

    void DrawLines(const D3D12FrameInfo& frameInfo);
    void DrawTriangles();
    void DrawText();
    void ClearLines();
    void ClearTriangles();
    void ClearText();

    JPH::Mutex _primitivesMutex{};
    Batch _emptyBatch{};

    JPH::Mutex _textMutex{};
    Vec<Text> _textArray{};

    JPH::Mutex _linesMutex{};
    Vec<Line> _lines{};
    u32 _linesPrimitivesIndex{ U32_INVALID_ID };
    
    ID3D12RootSignature* _triangleRootSig{ nullptr };
    ID3D12PipelineState* _trianglePSO{ nullptr };
    ID3D12RootSignature* _triangleWireframeRootSig{ nullptr };
    ID3D12PipelineState* _triangleWireframePSO{ nullptr };
    ID3D12RootSignature* _lineRootSig{ nullptr };
    ID3D12PipelineState* _linePSO{ nullptr };
};

void Initialize();
void Shutdown();
void Render(const D3D12FrameInfo& frameInfo);
void Clear();
DebugRenderer* const GetDebugRenderer();

}