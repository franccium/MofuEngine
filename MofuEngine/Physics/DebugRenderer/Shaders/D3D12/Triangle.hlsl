struct VSConstants
{
    float4x4 View;
    float4x4 Projection;
};
ConstantBuffer<VSConstants> Constants : register(b0, space0);

struct VSIn
{
    float3 Position : POSITION;
    float3 Color : COLOR;
};

struct VSOut
{
    float4 HomPosition : SV_Position;
    float4 Color : COLOR0;
};

struct PSOut
{
    float4 Color : SV_Target0;
};

VSOut TriangleVS(VSIn input)
{
    VSOut vsout;
    float4 pos = float4(input.Position, 1.f);
    pos = mul(Constants.View, pos);
    pos = mul(Constants.Projection, pos);
    vsout.HomPosition = pos;
    vsout.Color = float4(input.Color, 1.f);
    return vsout;
}

PSOut TrianglePS(VSOut input)
{
    PSOut psOut;
    psOut.Color = input.Color;
    return psOut;
}