#include "../MofuEngine/Graphics/D3D12/Shaders/Common.hlsli"

struct VertexOut
{
    float4 HomogenousPositon : SV_POSITION;
    float3 WorldPosition : POSITION;
    float3 WorldNormal : NORMAL;
    float3 WorldTangent : TANGENT;
    float2 UV : TEXTURE;
};

struct PixelOut
{
    float4 Color : SV_TARGET;
};

#define ElementsTypeStaticNormal 0x01
#define ElementsTypeStaticNormalTexture 0x03
#define ElementsTypeStaticColor 0x04
#define ElementsTypeSkeletal 0x08
#define ElementsTypeSkeletalColor ElementsTypeSkeletal | ElementsTypeStaticColor
#define ElementsTypeSkeletalNormal ElementsTypeSkeletal | ElementsTypeStaticNormal
#define ElementsTypeSkeletalNormalColor ElementsTypeSkeletalNormal | ElementsTypeStaticColor
#define ElementsTypeSkeletalNormalTexture ElementsTypeSkeletal | ElementsTypeStaticNormalTexture
#define ElementsTypeSkeletalNormalTextureColor ElementsTypeSkeletalNormalTexture | ElementsTypeStaticColor

struct VertexElement
{
#if ELEMENTS_TYPE == ElementsTypeStaticNormal
    uint ColorTSign;
    uint16_t2 Normal;
#elif ELEMENTS_TYPE == ElementsTypeStaticNormalTexture
    uint ColorTSign;
    uint16_t2 Normal;
    uint16_t2 Tangent;
    float2 UV;
#elif ELEMENTS_TYPE == ElementsTypeStaticColor
    uint8_t3 Color;
    float pad;
#elif ELEMENTS_TYPE == ElementsTypeSkeletal
#elif ELEMENTS_TYPE == ElementsTypeSkeletalColor
#elif ELEMENTS_TYPE == ElementsTypeSkeletalNormal
#elif ELEMENTS_TYPE == ElementsTypeSkeletalNormalColor
#elif ELEMENTS_TYPE == ElementsTypeSkeletalNormalTexture
#elif ELEMENTS_TYPE == ElementsTypeSkeletalNormalTextureColor
#endif
};

const static float InvIntervals = 2.f / ((1 << 16) - 1);

ConstantBuffer<GlobalShaderData> GlobalData : register(b0, space0);
ConstantBuffer<PerObjectData> PerObjectBuffer : register(b1, space0);
StructuredBuffer<float3> VertexPositions : register(t0, space0);
StructuredBuffer<VertexElement> Elements : register(t1, space0);

VertexOut TestShaderVS(in uint VertexIdx : SV_VertexID)
{
    VertexOut vsOut;
    
    float4 position = float4(VertexPositions[VertexIdx], 1.f);
    float4 worldPosition = mul(PerObjectBuffer.World, position);
    
#if ELEMENTS_TYPE == ElementsTypeStaticNormal
    VertexElement element = Elements[VertexIdx];
    float2 nXY = element.Normal * InvIntervals - 1.f;
    uint signs = (element.ColorTSign >> 24) & 0xff;
    uint16_t2 packedNormal = element.Normal;
    float nSign = float(signs & 0x02) - 1;
    float3 normal = float3(nXY.x, nXY.y, sqrt(saturate(1.f - dot(nXY, nXY))) * nSign);
    
    vsOut.HomogenousPositon = mul(PerObjectBuffer.WorldViewProjection, position);
    vsOut.WorldPosition = worldPosition.xyz;
    vsOut.WorldNormal = mul(float4(normal, 0.f), PerObjectBuffer.InvWorld).xyz;
    vsOut.WorldTangent = 0.f;
   
    vsOut.UV = 0.f;
    
#elif ELEMENTS_TYPE == ElementsTypeStaticNormalTexture

    VertexElement element = Elements[VertexIdx];
    float2 nXY = element.Normal * InvIntervals - 1.f;
    uint signs = (element.ColorTSign >> 24) & 0xff;
    uint16_t2 packedNormal = element.Normal;
    float nSign = float(signs & 0x02) - 1;
    float3 normal = float3(nXY.x, nXY.y, sqrt(saturate(1.f - dot(nXY, nXY))) * nSign);
    
    vsOut.HomogenousPositon = mul(PerObjectBuffer.WorldViewProjection, position);
    vsOut.WorldPosition = worldPosition.xyz;
    vsOut.WorldNormal = mul(float4(normal, 0.f), PerObjectBuffer.InvWorld).xyz;
    vsOut.WorldTangent = 0.f;
    vsOut.UV = 0.f;
#else
#undef ELEMENTS_TYPE
    vsOut.HomogenousPositon = mul(PerObjectBuffer.WorldViewProjection, position);
    vsOut.WorldPosition = worldPosition.xyz;
    vsOut.WorldNormal = 0.f;
    vsOut.WorldTangent = 0.f;
    vsOut.UV = 0.f;
#endif
    
    return vsOut;
}

float3 CalculateLighting(float3 N, float3 L, float3 V, float3 lightColor,
    float3 diffuse, float3 specular, float specularPow)
{
    float3 h = normalize(L + V);
    float cosTheta = saturate(dot(N, h));
    const float ndotl = saturate(dot(N, L));
    
    float3 diffuseTerm = diffuse * ndotl;
    float3 specularTerm = specular * pow(cosTheta, specularPow);
    
    return lightColor * (diffuseTerm + specularTerm);
}

[earlydepthstencil]
PixelOut TestShaderPS(in VertexOut psIn)
{
    PixelOut psOut;
    float3 normal = normalize(psIn.WorldNormal);
    float3 viewDir = normalize(GlobalData.CameraPosition - psIn.WorldPosition);
    float3 color = 0;
            
    float3 diffuseColor = float3(0.8, 0.5, 0.2);
    float3 specularColor = float3(0.9, 0.9, 0.9);
    float specularPower = 16.f;

    float3 lightDirection = float3(0.7f, 0.7f, -0.7f);
    float NdotL = saturate(dot(normal, lightDirection));
    float3 lightColor = float3(0.6f, 0.33f, 0.9f);
    color += CalculateLighting(normal, lightDirection, viewDir, lightColor, diffuseColor, specularColor, specularPower);
    
    float3 ambientColor = float3(0.1, 0.1, 0.1);
    color += ambientColor;
    color = saturate(color);
    
    psOut.Color = float4(color, 1.f);
    return psOut;
}