#pragma once
#include "Physics/JoltCommon.h"

#include <Jolt/Core/Mutex.h>
#include <Jolt/Core/UnorderedMap.h>
#include <Jolt/Renderer/DebugRenderer.h>
#include "Graphics/D3D12/D3D12CommonHeaders.h"
#include "Graphics/D3D12/D3D12Primitives.h"
#include "FontRenderer.h"

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
    void DrawText3D(JPH::Mat44Arg transform, const std::string_view& string, JPH::ColorArg color);

    void DrawGeometry(JPH::RMat44Arg modelMatrix, const JPH::AABox& worldSpaceBounds, f32 LODScaleSq, JPH::ColorArg modelColor, const GeometryRef& geometry, ECullMode cullMode, ECastShadow castShadow, EDrawMode drawMode) override;

    Batch CreateTriangleBatch(const Triangle* triangles, i32 triangleCount) override;
    Batch CreateTriangleBatch(const Vertex* vertices, i32 vertexCount, const u32* indices, i32 indexCount) override;

    void DrawTestText();

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
    // a batch of instances that have the same primitive
    struct InstanceBatch
    {
        Vec<Instance> Instances;
        Vec<u32> GeometryStartIndex; // start index in mInstancesBuffer for each of the LOD, length is one longer than the number of LODs to indicate how many instances the last lod has
    };
    struct Text
    {
        JPH::Vec3 Position;
        f32 Height;
        JPH::Color Color;
        JPH::String String;

        Text(JPH::Vec3Arg pos, f32 height, JPH::ColorArg color, const std::string_view& str) 
            : Position{ pos }, Height{ height }, Color{ color }, String{ str } {}
    };
    struct TextTransformed
    {
        JPH::Mat44 Transform;
        JPH::Color Color;
        JPH::String String;

        TextTransformed(JPH::Mat44Arg transform, JPH::ColorArg color, const std::string_view& str)
            : Transform{ transform }, Color{ color }, String{ str } {}
    };

    void DrawLines(const D3D12FrameInfo& frameInfo, D3D12_GPU_VIRTUAL_ADDRESS constants);
    void DrawTriangles(const D3D12FrameInfo& frameInfo, D3D12_GPU_VIRTUAL_ADDRESS constants);
    void DrawTextBuffer(const D3D12FrameInfo& frameInfo, D3D12_GPU_VIRTUAL_ADDRESS constants);
    void ClearLines();
    void ClearTriangles();
    void ClearTextBuffer();
    void DrawInstances(const Geometry* inGeometry, const InstanceBatch& instanceBatch, const D3D12FrameInfo& frameInfo, DXGraphicsCommandList* const cmdList);

    JPH::Mutex _primitivesMutex{};
    Batch _emptyBatch{};

    JPH::Mutex _textMutex{};
    Vec<Text> _textArray{};
    Vec<TextTransformed> _textArrayNonBillboard{};

    Vec<Text> _testText{};
    

    JPH::Mutex _linesMutex{};
    Vec<Line> _lines{};
    u32 _linesPrimitivesIndex{ U32_INVALID_ID };
    
    ID3D12RootSignature* _triangleRootSig{ nullptr };
    ID3D12PipelineState* _trianglePSO{ nullptr };
    ID3D12RootSignature* _triangleWireframeRootSig{ nullptr };
    ID3D12PipelineState* _triangleWireframePSO{ nullptr };
    ID3D12RootSignature* _lineRootSig{ nullptr };
    ID3D12PipelineState* _linePSO{ nullptr };

    using InstanceMap = HashMap<GeometryRef, InstanceBatch>;
    InstanceMap _wireframePrimitives{};
    InstanceMap _solidPrimitives{};
    InstanceMap _tempPrimitives{};
    InstanceMap _backfacingPrimitives{};
    u32 _instanceCount{ 0 };

    font::FontRenderer _font{};
};

void Initialize();
void Shutdown();
void Render(const D3D12FrameInfo& frameInfo);
void Clear();
void DrawBodyShape(const JPH::Shape* shape, JPH::RMat44Arg centerOfMassTransform, JPH::Vec3 scale, bool wireframe = true);
DebugRenderer* const GetDebugRenderer();

}