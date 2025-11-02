struct VSConstants
{
    float4x4 ViewProjection;
};
ConstantBuffer<VSConstants> Constants : register(b0, space0);
Texture2D Texture : register(t0, space0);
SamplerState Sampler : register(s0, space0);

struct VSIn
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float2 PAD : PAD;
};

struct VSOut
{
    float4 HomPosition : SV_Position;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
};

struct PSOut
{
    float4 Color : SV_Target0;
};

VSOut FontVS(VSIn input)
{
    VSOut vsout;
    float4 pos = float4(input.Position, 1.f);
    pos = mul(Constants.ViewProjection, pos);
    vsout.HomPosition = pos;
    vsout.Color = input.Color;
    vsout.UV = input.UV;
    return vsout;
}

PSOut FontPS(VSOut input)
{
    PSOut psOut;
    float a = Texture.Sample(Sampler, input.UV).r;
    if (a < 0.5)
        discard;
    
    psOut.Color = float4(input.Color.rgb, a);
    return psOut;
}