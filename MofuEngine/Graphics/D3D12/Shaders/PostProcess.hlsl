
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
    Texture2D gpassMain = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
    float4 color = float4(gpassMain[Position.xy].xyz, 1.f);
    return color + 0.5f;
}