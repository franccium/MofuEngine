#include "../MofuEngine/Graphics/D3D12/Shaders/Common.hlsli"

struct VertexOut
{
    float4 HomogenousPositon : SV_POSITION;
    float3 WorldPosition : POSITION;
    float3 WorldNormal : NORMAL;
    float4 WorldTangent : TANGENT; // r - handedness
    float2 UV : TEXTURE;
};

struct PixelOut
{
    float4 Color : SV_TARGET;
};

struct Surface
{
    float3 BaseColor;
    float Metallic;
    float3 Normal;
    float PerceptualRoughness;
    float3 EmissiveColor;
    float EmissiveIntensity;
    float3 V;
    float AmbientOcclusion;
    float3 DiffuseColor;
    float a2; // Pow(PerceptualRoughness, 2)
    float3 SpecularColor;
    float NoV;
    float SpecularStrength;
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
StructuredBuffer<uint> SrvIndices : register(t2, space0);

SamplerState PointSampler : register(s0, space0);
SamplerState LinearSampler : register(s1, space0);
SamplerState AnisotropicSampler : register(s2, space0);

float4 Sample(uint index, SamplerState s, float2 uv)
{
    return Texture2D(ResourceDescriptorHeap[index]).Sample(s, uv);
}

float4 Sample(uint index, SamplerState s, float2 uv, float mip)
{
    return Texture2D(ResourceDescriptorHeap[index]).SampleLevel(s, uv, mip);
}

float4 Sample(uint index, SamplerState s, float3 n)
{
    return TextureCube(ResourceDescriptorHeap[index]).Sample(s, n);
}

float4 Sample(uint index, SamplerState s, float3 n, float mip)
{
     return TextureCube(ResourceDescriptorHeap[index]).SampleLevel(s, n, mip);
}

float3 PhongBRDF(float3 N, float3 L, float3 V, float3 diffuseColor, float3 specularColor, float shininess)
{
    float3 color = diffuseColor;
    const float3 R = reflect(-L, N);
    const float VoR = max(dot(V, R), 0.f);
    color += pow(VoR, max(shininess, 1.f)) * specularColor;
    
    return color;
}

//float3 CookTorranceBRDF(Surface S, float3 L)
//{
//    const float3 N = S.Normal;
//    const float3 H = normalize(S.V + L); // halfway vector of view and light dir, microsurface normal
//    const float NoV = abs(S.NoV) + 1e-5f; // make sure its not 0 for divisions
//    const float NoL = saturate(dot(N, L));
//    const float NoH = saturate(dot(N, H));
//    const float VoH = saturate(dot(S.V, H));

//    const float D = D_GGX(NoH, S.a2);
//    const float G = V_SmithGGXCorrelated(NoV, NoL, S.a2);
//    const float3 F = F_Schlick(S.SpecularColor, VoH);

//    float3 specularBRDF = (D * G) * F;
//    float3 rho = 1.f - F;
//    float3 diffuseBRDF = Diffuse_Lambert() * S.DiffuseColor * rho;
//    //float3 diffuseBRDF = Diffuse_Burley(NoV, NoL, VoH, S.PerceptualRoughness * S.PerceptualRoughness) * S.DiffuseColor * rho;

//    float2 brdfLut = Sample(GlobalData.AmbientLight.BrdfLutSrvIndex, LinearSampler, float2(NoV, S.PerceptualRoughness), 0).rg;
//    float3 energyLossCompensation = 1.f + S.SpecularColor * (rcp(brdfLut.x) - 1.f);
//    specularBRDF *= energyLossCompensation;

//    return (diffuseBRDF + specularBRDF * S.SpecularStrength) * NoL;
//}

VertexOut TestShaderVS(in uint VertexIdx : SV_VertexID)
{
    VertexOut vsOut;
    
    float4 position = float4(VertexPositions[VertexIdx], 1.f);
    float4 worldPosition = mul(PerObjectBuffer.World, position);
    
#if ELEMENTS_TYPE == ElementsTypeStaticNormal
    VertexElement element = Elements[VertexIdx];
    uint signs = element.ColorTSign >> 24;
    
    uint16_t2 packedNormal = element.Normal;
    float nSign = float((signs & 0x04) >> 1) - 1.f;
    float2 nXY = element.Normal * InvIntervals - 1.f;
    float3 normal = float3(nXY, sqrt(saturate(1.f - dot(nXY, nXY))) * nSign);
    
    vsOut.HomogenousPositon = mul(PerObjectBuffer.WorldViewProjection, position);
    vsOut.WorldPosition = worldPosition.xyz;
    vsOut.WorldNormal = mul(float4(normal, 0.f), PerObjectBuffer.InvWorld).xyz;
    vsOut.WorldTangent = 0.f;
   
    vsOut.UV = 0.f;
    
#elif ELEMENTS_TYPE == ElementsTypeStaticNormalTexture
    VertexElement element = Elements[VertexIdx];
    uint signs = element.ColorTSign >> 24;
    float tSign = float((signs & 0x02) - 1.f); // we get +1.f if set, -1.f if not set
    float handSign = float((signs & 0x01) << 1) - 1.f;
    float nSign = float((signs & 0x04) >> 1) - 1.f;
    
    uint16_t2 packedNormal = element.Normal;
    float2 nXY = element.Normal * InvIntervals - 1.f;
    float3 normal = float3(nXY, sqrt(saturate(1.f - dot(nXY, nXY))) * nSign);
    
    float2 tXY = element.Tangent * InvIntervals - 1.f;
    float3 tangent = float3(tXY, sqrt(saturate(1.f - dot(tXY, tXY))) * tSign);
    tangent = tangent - normal * dot(normal, tangent); // use Gram-Schmidt orthogonalization to restore orthogonality
    
    vsOut.HomogenousPositon = mul(PerObjectBuffer.WorldViewProjection, position);
    vsOut.WorldPosition = worldPosition.xyz;
    vsOut.WorldNormal = normalize(mul(normal, (float3x3)PerObjectBuffer.InvWorld));
    vsOut.WorldTangent = float4(normalize(mul(tangent, (float3x3)PerObjectBuffer.InvWorld)), handSign);
    vsOut.UV = element.UV;
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

Surface GetSurface(VertexOut psIn, float3 V)
{
    Surface S;

    /*
        BaseColor = 0,
		Normal,
		MetallicRoughness,
		Emissive,
		AmbientOcclusion,
    */
#if 0
    float2 uv = psIn.UV;
    S.AmbientOcclusion = 0.1f;
    S.BaseColor = Sample(SrvIndices[0], LinearSampler, uv).rgb;
    S.EmissiveColor = 0.3f;
    float2 metalRough = float2(0.3f, 0.7f);
    S.Metallic = metalRough.r;
    S.PerceptualRoughness = metalRough.g;
    S.EmissiveIntensity = 1.f;
    
    float3 n = Sample(SrvIndices[1], LinearSampler, uv).rgb;
    n = n * 2.f - 1.f;
    n.z = sqrt(1.f - saturate(dot(n.xy, n.xy)));
    
    const float3 N = psIn.WorldNormal;
    const float3 T = psIn.WorldTangent.xyz;
    const float3 B = cross(N, T) * psIn.WorldTangent.w;
    const float3x3 TBN = float3x3(T, B, N);
    // transform from tangent-space to world-space
    S.Normal = normalize(mul(n, TBN));
#else
#if TEXTURED_MTL
    //float2 uv = psIn.UV;
    //S.AmbientOcclusion = Sample(SrvIndices[4], LinearSampler, uv).r;
    //S.BaseColor = Sample(SrvIndices[0], LinearSampler, uv).rgb;
    //S.EmissiveColor = Sample(SrvIndices[3], LinearSampler, uv).rgb;
    //float2 metalRough = Sample(SrvIndices[2], LinearSampler, uv).rg;
    //S.Metallic = metalRough.r;
    //S.PerceptualRoughness = metalRough.g;
    //S.EmissiveIntensity = 1.f;
    
    //float3 n = Sample(SrvIndices[1], LinearSampler, uv).rgb;
    //n = n * 2.f - 1.f;
    //n.z = sqrt(1.f - saturate(dot(n.xy, n.xy)));
    
    //const float3 N = psIn.WorldNormal;
    //const float3 T = psIn.WorldTangent.xyz;
    //const float3 B = cross(N, T) * psIn.WorldTangent.w;
    //const float3x3 TBN = float3x3(T, B, N);
    //// transform from tangent-space to world-space
    //S.Normal = normalize(mul(n, TBN));
    
    float2 uv = psIn.UV;
    S.AmbientOcclusion = 0.1f;
    S.BaseColor = Sample(SrvIndices[0], LinearSampler, uv).rgb;
    S.EmissiveColor = 0.3f;
    float2 metalRough = float2(0.3f, 0.7f);
    S.Metallic = metalRough.r;
    S.PerceptualRoughness = metalRough.g;
    S.EmissiveIntensity = 1.f;
    
    float3 n = Sample(SrvIndices[1], LinearSampler, uv).rgb;
    n = n * 2.f - 1.f;
    n.z = sqrt(1.f - saturate(dot(n.xy, n.xy)));
    
    const float3 N = psIn.WorldNormal;
    const float3 T = psIn.WorldTangent.xyz;
    const float3 B = cross(N, T) * psIn.WorldTangent.w;
    const float3x3 TBN = float3x3(T, B, N);
    // transform from tangent-space to world-space
    S.Normal = normalize(mul(n, TBN));
    
#else
/*
    S.BaseColor = 1.f;
    S.Metallic = 0.f;
    S.Normal = normalize(psIn.WorldNormal);
    S.PerceptualRoughness = 1.f;
    S.EmissiveColor = 0.f;
    S.EmissiveIntensity = 1.f;
    S.AmbientOcclusion = 1.f;*/
    S.BaseColor = PerObjectBuffer.BaseColor.rgb;
    S.Metallic = PerObjectBuffer.Metallic;
    S.Normal = normalize(psIn.WorldNormal);
    S.PerceptualRoughness = PerObjectBuffer.Roughness;
    S.EmissiveColor = PerObjectBuffer.Emissive;
    S.EmissiveIntensity = PerObjectBuffer.EmissiveIntensity;
    S.AmbientOcclusion = PerObjectBuffer.AmbientOcclusion;
#endif
    #endif
    S.V = V;
    S.PerceptualRoughness = max(S.PerceptualRoughness, 0.045f); // we limit the minimum to 0.045 to avoid artifacts caused by the a2 term being too small
    const float roughness = S.PerceptualRoughness * S.PerceptualRoughness;
    S.a2 = roughness * roughness;
    S.NoV = dot(V, S.Normal);
    S.DiffuseColor = S.BaseColor * (1.f - S.Metallic);
    S.SpecularColor = lerp(0.04f, S.BaseColor, S.Metallic); // F0
    S.SpecularStrength = lerp(1 - min(S.PerceptualRoughness, 0.95f), 1.f, S.Metallic);

    return S;
}

// gradually shifts the light vector towards the surface normal as the surface becomes more rough
float3 GetSpecularDominantDir(float3 N, float3 R, float roughness)
{
    float smoothness = saturate(1.f - roughness);
    float t = smoothness * (sqrt(smoothness) + roughness);
    // the result is not normalized as we feitch in a cubemap
    return lerp(N, R, t);
}

float3 CalculateLighting(Surface S, float3 L, float3 lightColor)
{
#if 1 // PHONG
    const float NoL = saturate(dot(S.Normal, L));
    float3 color = PhongBRDF(S.Normal, L, S.V, S.DiffuseColor, 1.f, (1 - S.PerceptualRoughness) * 100.f) * (NoL / PI) * lightColor;
#else // PBR
    float3 color = CookTorranceBRDF(S, L) * lightColor;
#endif

    //TODO: we don't have light-units so we don't know what intensity value of 1 corresponds to, for now, multiplied by PI to make the scene lighter
    color *= PI;
    return color;
}

[earlydepthstencil]
PixelOut TestShaderPS(in VertexOut psIn)
{
    PixelOut psOut;
    float3 normal = normalize(psIn.WorldNormal);
    float3 viewDir = normalize(GlobalData.CameraPosition - psIn.WorldPosition);
    float3 color = 0;
    Surface S = GetSurface(psIn, viewDir);
            
    float3 diffuseColor = float3(1.0, 0.8, 0.8);
    float3 specularColor = float3(0.9, 0.9, 0.9);
    float specularPower = 16.f;

    float3 lightDirection = float3(0.7f, 0.7f, -0.2f);
    float NdotL = saturate(dot(normal, lightDirection));
    float3 lightColor = float3(0.98f, 0.99f, 0.98f);
    float lightIntensity = 1.f;
    color += CalculateLighting(S, -lightDirection, lightColor * lightIntensity);
    
    float3 ambientColor = float3(0.1, 0.1, 0.1);
    color += ambientColor;
    color = saturate(color);
    
    #if TEXTURED_MTL
    float VoN = S.NoV * 1.3f;
    float VoN2 = VoN * VoN;
    float VoN4 = VoN2 * VoN2;
    float3 e = S.EmissiveColor;
    S.EmissiveColor = max(VoN4 * VoN4, 0.1f) * e * e;
#endif
    
    //color.rgb = pow(color.rgb, 2.2f);
    psOut.Color = float4(color, 1.f);
    
    
    return psOut;
}