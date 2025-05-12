#include "Common.hlsli"

struct ShaderConstants
{
    float Width;
    float Height;
    uint Frame;
};

ConstantBuffer<ShaderConstants> ShaderParams : register(b1, space0);

float4 ColorFillPS(in noperspective float4 Position : SV_Position, in noperspective float2 UV : TEXCOORD) : SV_Target0
{
    const float2 invDim = float2(1.f / ShaderParams.Width, 1.f / ShaderParams.Height);
    const float2 uv = (Position.xy) * invDim;
    return float4(uv, 0.0f, 1.f);
}