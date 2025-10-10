
#include "Common.hlsli"

#ifdef DEBUG
struct DebugConstants
{
    uint DebugMode; // 0 - default; 1 - depth; 2 - normals; 3 - material IDs
};

#define DEBUG_DEFAULT 0
#define DEBUG_DEPTH 1
#define DEBUG_NORMALS 2
#define DEBUG_MATERIAL_IDS 3
#endif

struct PostProcessConstants
{
    uint GPassMainBufferIndex;
    uint GPassDepthBufferIndex;
    uint NormalBufferIndex;
    uint RTBufferIndex;
};

ConstantBuffer<GlobalShaderData> GlobalData : register(b0, space0);
ConstantBuffer<PostProcessConstants> ShaderParams : register(b1, space0);
#ifdef DEBUG
ConstantBuffer<DebugConstants> DebugOptions : register(b2, space0);
#endif
SamplerState PointSampler : register(s0, space0);
SamplerState LinearSampler : register(s1, space0);

#if RAYTRACING
float4 PostProcessPS(in noperspective float4 Position : SV_Position, in noperspective float2 UV : TEXCOORD) : SV_TARGET0
{
    Texture2D gpassMain = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
    Texture2D rtMain = ResourceDescriptorHeap[ShaderParams.RTBufferIndex];
    float4 gpassColor = float4(gpassMain[Position.xy].rgb, 1.f);
    float4 rtColor = float4(rtMain[Position.xy].rgb, 1.f);
    float4 fin = lerp(gpassColor, rtColor, 0.5f);
    return fin;
}
#else
float4 PostProcessPS(in noperspective float4 Position : SV_Position, in noperspective float2 UV : TEXCOORD) : SV_TARGET0
{
    Texture2D gpassDepth = ResourceDescriptorHeap[ShaderParams.GPassDepthBufferIndex];
    float depth = gpassDepth[Position.xy].r;
    if (depth > 0.f)
    {
#ifdef DEBUG
        Texture2D texture;
        float3 color;
        if (DebugOptions.DebugMode == DEBUG_DEFAULT)
        {
            texture = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
            color = texture[Position.xy].rgb;
        }
        else if (DebugOptions.DebugMode == DEBUG_DEPTH)
        {
            texture = ResourceDescriptorHeap[ShaderParams.GPassDepthBufferIndex];
            color = texture[Position.xy].rrr * 150.f;
        }
        else if (DebugOptions.DebugMode == DEBUG_NORMALS)
        {
            texture = ResourceDescriptorHeap[ShaderParams.NormalBufferIndex];
            color = texture[Position.xy].rgb;
        }
        else if (DebugOptions.DebugMode == DEBUG_MATERIAL_IDS)
        {
            texture = ResourceDescriptorHeap[ShaderParams.NormalBufferIndex];
            color = texture[Position.xy].aaa;
        }
        color = saturate(color);
        return float4(color, 1.f);
#else
        Texture2D gpassMain = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
        return float4(gpassMain[Position.xy].rgb, 1.f);
#endif
    }
    else
    {
        float4 clip = float4(2.f * UV.x - 1.f, -2.f * UV.y + 1.f, 0.f, 1.f);
        float3 view = mul(GlobalData.InvProjection, clip).xyz;
        float3 direction = mul(view, (float3x3) GlobalData.View); // we swap the order or operations so the view matrix is transposed, since rotation is orthogonal, this makes it inverse view
        return TextureCube( ResourceDescriptorHeap[GlobalData.AmbientLight.DiffuseSrvIndex])
            .SampleLevel(LinearSampler, direction, 0.1f) * GlobalData.AmbientLight.Intensity;
    }
}
#endif