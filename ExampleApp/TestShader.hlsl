#include "../MofuEngine/Graphics/D3D12/Shaders/Common.hlsli"
#include "BRDF.hlsli"

struct VertexOut
{
    float4 HomogenousPositon : SV_POSITION;
    float3 WorldPosition : POSITION;
    float3 WorldNormal : NORMAL;
    float4 WorldTangent : TANGENT; // z - handedness
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
    float3 V; // view direction
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
    uint ColorTSign; // rgb - color, z - tangent and normal signs
    uint16_t2 Normal;
#elif ELEMENTS_TYPE == ElementsTypeStaticNormalTexture
    uint ColorTSign; // rgb - color, z - tangent and normal signs
    float2 UV;
    uint16_t2 Normal;
    uint16_t2 Tangent;
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

StructuredBuffer<DirectionalLightParameters> DirectionalLights : register(t3, space0);
StructuredBuffer<CullableLightParameters> CullableLights : register(t4, space0);
StructuredBuffer<uint2> LightGrid : register(t5, space0);
StructuredBuffer<uint> LightIndexList : register(t6, space0);

SamplerState PointSampler : register(s0, space0);
SamplerState LinearSampler : register(s1, space0);
SamplerState AnisotropicSampler : register(s2, space0);

#define TILE_SIZE 32

float4 Sample(uint index, SamplerState s, float2 uv)
{
    return Texture2D(ResourceDescriptorHeap[index]).Sample(s, uv);
}

float4 Sample(uint index, SamplerState s, float2 uv, float mip)
{
    return Texture2D(ResourceDescriptorHeap[index]).SampleLevel(s, uv, mip);
}

float4 SampleCube(uint index, SamplerState s, float3 n)
{
    return TextureCube(ResourceDescriptorHeap[index]).Sample(s, n);
}

float4 SampleCube(uint index, SamplerState s, float3 n, float mip)
{
     return TextureCube(ResourceDescriptorHeap[index]).SampleLevel(s, n, mip);
}

float3 PhongBRDF(float3 N, float3 L, float3 V, float3 diffuseColor, float3 specularColor, float shininess)
{
    const float3 R = reflect(-L, N);
    const float VoR = max(dot(V, R), 0.f);
    
    return diffuseColor + pow(VoR, max(shininess, 1.f)) * specularColor;
}

float3 CookTorranceBRDF(Surface S, float3 L)
{
    const float3 N = S.Normal;
    const float3 H = normalize(S.V + L); // halfway vector of view and light dir, microsurface normal
    const float NoV = abs(S.NoV) + 1e-5f; // make sure its not 0 for divisions
    const float NoL = saturate(dot(N, L));
    const float NoH = saturate(dot(N, H));
    const float VoH = saturate(dot(S.V, H));

    const float D = D_GGX(NoH, S.a2);
    const float G = V_SmithGGXCorrelated(NoV, NoL, S.a2);
    const float3 F = F_Schlick(S.SpecularColor, VoH);

    float3 specularBRDF = (D * G) * F;
    float3 rho = 1.f - F;
    //float3 diffuseBRDF = Diffuse_Lambert() * S.DiffuseColor * rho;
    float3 diffuseBRDF = Diffuse_Burley(NoV, NoL, VoH, S.PerceptualRoughness * S.PerceptualRoughness) * S.DiffuseColor * rho;

    float2 brdfLut = Sample(GlobalData.AmbientLight.BrdfLutSrvIndex, LinearSampler, float2(NoV, S.PerceptualRoughness), 0).rg;
    float3 energyLossCompensation = 1.f + S.SpecularColor * (rcp(brdfLut.x) - 1.f);
    specularBRDF *= energyLossCompensation;

    return (diffuseBRDF + specularBRDF * S.SpecularStrength) * NoL;
}

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
    float2 uv = psIn.UV;
    S.AmbientOcclusion = Sample(SrvIndices[4], LinearSampler, uv).r;
    S.BaseColor = Sample(SrvIndices[0], LinearSampler, uv).rgb;
    S.EmissiveColor = Sample(SrvIndices[3], LinearSampler, uv).rgb;
    float2 metalRough = Sample(SrvIndices[2], LinearSampler, uv).rg;
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
    // the result is not normalized as we fetch in a cubemap
    return lerp(N, R, t);
}

float3 CalculateLighting(Surface S, float3 L, float3 lightColor)
{
#if 0 // PHONG
    const float NoL = saturate(dot(S.Normal, L));
    float3 color = PhongBRDF(S.Normal, L, S.V, S.DiffuseColor, 1.f, (1.f - S.PerceptualRoughness) * 100.f) * (NoL / PI) * lightColor;
#else // PBR
    float3 color = CookTorranceBRDF(S, L) * lightColor;
#endif

    //TODO: we don't have light-units so we don't know what intensity value of 1 corresponds to, for now, multiplied by PI to make the scene lighter
    color *= PI;
    return color;
}

float3 CalculatePointLight(Surface S, float3 worldPos, CullableLightParameters light)
{
    float3 L = light.Position - worldPos;
    const float dSq = dot(L, L);
    float3 color = 0.f;
    const float rSq = light.Range * light.Range;
    if(dSq < rSq)
    {
        const float dRcp = rsqrt(dSq);
        L *= dRcp; // normalize the light vector
        
        const float decaySpeed = 0.3f;
        float lDistNormalized = dSq / rSq;
        const float attenuation = pow((1 - lDistNormalized), 2) / (1 + decaySpeed * lDistNormalized);
        color = CalculateLighting(S, L, light.Color * light.Intensity * attenuation);
    }
    
    return color;
}

float3 CalculateSpotLight(Surface S, float3 worldPos, CullableLightParameters light)
{
    float3 L = light.Position - worldPos;
    const float dSq = dot(L, L);
    float3 color = 0.f;
    
    if (dSq < light.Range * light.Range)
    {
        const float dRcp = rsqrt(dSq);
        L *= dRcp;
        const float cosAngleToLight = saturate(dot(-L, light.Direction));
        const float attenuation = 1.f - smoothstep(-light.Range, light.Range, rcp(dRcp));
        const float angularAttenuation = smoothstep(light.CosPenumbra, light.CosUmbra, cosAngleToLight);
        color = CalculateLighting(S, L, light.Color * angularAttenuation * attenuation * light.Intensity);
    }
    
    return color;
}

float3 EvaluateIBL(Surface S)
{
    const float NoV = saturate(S.NoV);
    const float3 F0 = S.SpecularColor;
    const float3 F90 = max((1.f - S.PerceptualRoughness), F0);
    const float3 F = F_Schlick(NoV, F0, F90);
    
    const float roughness = S.PerceptualRoughness * S.PerceptualRoughness;
    AmbientLightParameters IBL = GlobalData.AmbientLight;
    float3 diffN = S.Normal;
    float3 diffuse = SampleCube(IBL.DiffuseSrvIndex, LinearSampler, diffN).rgb * S.DiffuseColor * (1.f - F);
    float3 specN = GetSpecularDominantDir(S.Normal, reflect(-S.V, S.Normal), roughness);
    float3 specularIBL = SampleCube(IBL.SpecularSrvIndex, LinearSampler, specN, S.PerceptualRoughness * 5.f).rgb; // mip level based on roughness
    float2 brdfLut = Sample(IBL.BrdfLutSrvIndex, LinearSampler, float2(NoV, S.PerceptualRoughness), 0).rg;
    float3 specular = specularIBL * (S.SpecularStrength * F0 * brdfLut.x + F90 * brdfLut.y);

    float3 energyLossCompensation = 1.f + F0 * (rcp(brdfLut.x) - 1.f);
    specular *= energyLossCompensation;

    return (diffuse + specular) * IBL.Intensity;
}

uint GetGridIndex(float2 posXY, float viewWidth)
{
    const uint2 pos = uint2(posXY);
    const uint tilesX = ceil(viewWidth / TILE_SIZE);
    return (pos.x / TILE_SIZE) + (tilesX * (pos.y / TILE_SIZE));
}

float3 Heatmap(StructuredBuffer<uint2> buffer, float2 posXY, float blend, float3 currentColor)
{
    const float w = GlobalData.ViewWidth;
    const uint gridIndex = GetGridIndex(posXY, w);
    uint numLights = buffer[gridIndex].y;
    const uint numPointLights = numLights >> 16;
    const uint numSpotLights = numLights & 0xffff;
    numLights = numPointLights + numSpotLights;
    
    const float3 mapTex[] =
    {
        float3(0, 0, 0),
        float3(0, 0, 1),
        float3(0, 1, 1),
        float3(0, 1, 0),
        float3(1, 1, 0),
        float3(1, 0, 0)
    };
    const uint mapTexLen = 5;
    const uint maxHeat = 40;
    float l = saturate((float) numLights / maxHeat) * mapTexLen;
    float3 a = mapTex[floor(l)];
    float3 b = mapTex[ceil(l)];
    float3 heatMap = lerp(a, b, l - floor(l));
    
    float3 color = lerp(currentColor.xyz, heatMap, blend);
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
    
    uint i = 0;
    for (i = 0; i < GlobalData.DirectionalLightsCount; ++i)
    {
        DirectionalLightParameters light = DirectionalLights[i];
        
        color += CalculateLighting(S, -light.Direction, light.Color * light.Intensity);
    }
  
    
    //for (i = 0; i < 8; ++i)
    //{
    //    CullableLightParameters light = CullableLights[i];
        
    //    color += CalculatePointLight(S, psIn.WorldPosition, light);
    //}
    
    //for (i = 8; i < 9; ++i)
    //{
    //    CullableLightParameters sLight = CullableLights[i];
    //    color += CalculateSpotLight(S, psIn.WorldPosition, sLight);
    //}
    
    const uint gridIndex = GetGridIndex(psIn.HomogenousPositon.xy, GlobalData.ViewWidth);
    uint lightStartIndex = LightGrid[gridIndex].x;
    const uint lightCount = LightGrid[gridIndex].y;
    const uint maxPointLight = lightStartIndex + (lightCount >> 16);
    const uint maxSpotLight = lightStartIndex + (lightCount & 0xFFFF);
    for (i = lightStartIndex; i < maxPointLight; ++i)
    {
        const uint lightIndex = LightIndexList[i];
        CullableLightParameters light = CullableLights[lightIndex];
        color += CalculatePointLight(S, psIn.WorldPosition, light);
    }
    for (i = maxPointLight; i < maxSpotLight; ++i)
    {
        const uint lightIndex = LightIndexList[i];
        CullableLightParameters light = CullableLights[lightIndex];
        color += CalculateSpotLight(S, psIn.WorldPosition, light);
    }
    
    float3 ambientColor = float3(0.1, 0.1, 0.1);
    //color += ambientColor;
    color = saturate(color);
    
#if TEXTURED_MTL
    float VoN = S.NoV * 1.3f;
    float VoN2 = VoN * VoN;
    float VoN4 = VoN2 * VoN2;
    float3 e = S.EmissiveColor;
    S.EmissiveColor = max(VoN4 * VoN4, 0.1f) * e * e;
#endif
    
    color += EvaluateIBL(S);

#if 0 // Light Grid
    float4 Position = psIn.HomogenousPositon;
    color = Heatmap(LightGrid, Position.xy, 0.1f, color);
#elif 1 // Scene
#endif
    
    psOut.Color = float4(color, 1.f);
    
    return psOut;
}