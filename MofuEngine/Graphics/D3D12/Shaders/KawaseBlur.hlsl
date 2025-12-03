
#include "Common.hlsli"

struct BlurParams
{
    uint TargetTexIndex;
};

ConstantBuffer<BlurParams> Params : register(b0, space0);
SamplerState LinearSampler : register(s0, space0);

static const float2 SSILVB_Res = float2(800.f, 450.f);
static const float2 SSILVB_HalfRes = float2(400.f, 225.f);
//TODO: one pass compute?
float4 KawaseBlurDown(in noperspective float4 Position : SV_Position, in noperspective float2 UV : TEXCOORD) : SV_TARGET0
{
    Texture2D tex = ResourceDescriptorHeap[Params.TargetTexIndex];
    
    //float2 offset = float2(1.5f / SSILVB_Res.x, 1.5f / SSILVB_Res.y);
    float2 offset = float2(1.5f / SSILVB_Res.x, 1.5f / SSILVB_Res.y);
    float2 inputUV = UV * 2.f;

    float4 col = 0;
    col += tex.Sample(LinearSampler, inputUV + offset * float2(1, 1));
    col += tex.Sample(LinearSampler, inputUV + offset * float2(-1,  1));
    col += tex.Sample(LinearSampler, inputUV + offset * float2( 1, -1));
    col += tex.Sample(LinearSampler, inputUV + offset * float2(-1, -1));
    
    return col * 0.25f;
}

float4 KawaseBlurUp(in noperspective float4 Position : SV_Position, in noperspective float2 UV : TEXCOORD) : SV_TARGET0
{
    Texture2D tex = ResourceDescriptorHeap[Params.TargetTexIndex];
    float2 offset = float2(1.5f / SSILVB_HalfRes.x, 1.5f / SSILVB_HalfRes.y) * 0.2f;
    float2 inputUV = UV / 2.f;

    float4 col = 0;
    col += tex.Sample(LinearSampler, inputUV + offset * float2(1, 1));
    col += tex.Sample(LinearSampler, inputUV + offset * float2(-1, 1));
    col += tex.Sample(LinearSampler, inputUV + offset * float2(1, -1));
    col += tex.Sample(LinearSampler, inputUV + offset * float2(-1, -1));
    
    return col * 0.25f;
}
