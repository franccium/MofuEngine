
#include "Common.hlsli"

struct PostProcessConstants
{
    uint GPassMainBufferIndex;
    uint GPassDepthBufferIndex;
};

ConstantBuffer<GlobalShaderData> GlobalData : register(b0, space0);
ConstantBuffer<PostProcessConstants> ShaderParams : register(b1, space0);
SamplerState PointSampler : register(s0, space0);
SamplerState LinearSampler : register(s1, space0);

float4 PostProcessPS(in noperspective float4 Position : SV_Position, in noperspective float2 UV : TEXCOORD) : SV_TARGET0
{
    #if 0
    Texture2D gpassMain = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
    float4 color = float4(gpassMain[Position.xy].xyz, 1.f);
    return color;
#else
    Texture2D gpassDepth = ResourceDescriptorHeap[ShaderParams.GPassDepthBufferIndex];
    float depth = gpassDepth[Position.xy].r;
    if (depth > 0.f)
    {
        Texture2D gpassMain = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
        return float4(gpassMain[Position.xy].xyz, 1.f);
    }
    else
    {
        float4 clip = float4(2.f * UV.x - 1.f, -2.f * UV.y + 1.f, 0.f, 1.f);
        float3 view = mul(GlobalData.InvProjection, clip).xyz;
        float3 direction = mul(view, (float3x3) GlobalData.View); // we swap the order or operations so the view matrix is transposed, since rotation is orthogonal, this makes it inverse view
        return TextureCube( ResourceDescriptorHeap[GlobalData.AmbientLight.SpecularSrvIndex])
            .SampleLevel(LinearSampler, direction, 0.1f) * GlobalData.AmbientLight.Intensity;
    }
#endif
}