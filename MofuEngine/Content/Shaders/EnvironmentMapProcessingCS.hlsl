
#include "ContentTypes.hlsli"

static const float PI = 3.14159265f;
static const float INV_PI = 0.31830988618f;
static const float INV_TAU = 0.15915494309f;
static const float SAMPLE_OFFSET = 0.5f;

ConstantBuffer<EnvironmentProcessingConstants> Constants : register(b0, space0);
RWStructuredBuffer<float4> Output : register(u0, space0);
SamplerState LinearSampler : register(s0, space0);

float2 Hammersley(uint i, uint N)
{
    i = (i << 16u) | (i >> 16u);
    i = ((i & 0x55555555u) << 1u) | ((i & 0xAAAAAAAAu) >> 1u);
    i = ((i & 0x33333333u) << 2u) | ((i & 0xCCCCCCCCu) >> 2u);
    i = ((i & 0x0F0F0F0Fu) << 4u) | ((i & 0xF0F0F0F0u) >> 4u);
    i = ((i & 0x00FF00FFu) << 8u) | ((i & 0xFF00FF00u) >> 8u);
    float fi = float(i);
    return float2(fi / float(N), fi * 2.3283064365386963e-10);
}

float3 GetSampleDirEquirectangular(uint face, float x, float y)
{
    float3 directions[6] =
    {
        { -x, 1.f, -y }, // x+ left
        { x, -1.f, -y }, // x- right
        { y, x, 1.f }, // y+ botton
        { -y, x, -1.f }, // y- top
        { 1.f, x, -y }, // z+ front
        { -1.f, -x, -y } // z- back
    };

    return normalize(directions[face]);
}

float3 GetSampleDirCubemap(uint face, float x, float y)
{
    float3 directions[6] =
    {
        { 1.f, -y, -x }, // x+ left
        { -1.f, -y, x }, // x- right
        { x, 1.f, y }, // y+ botton
        { x, -1.f, -y }, // y- top
        { x, -y, 1.f }, // z+ front
        { -x, -y, -1.f } // z- back
    };

    return normalize(directions[face]);
}

float2 DirectionEquirectangularUV(float3 dir)
{
    float Phi = atan2(dir.y, dir.x);
    float Theta = acos(dir.z);
    float u = Phi * INV_TAU + 0.5f;
    float v = Theta * INV_PI;
    return float2(u, v);
}

float3x3 GetTangentFrame(float3 normal)
{
    float3 up = abs(normal.z) < 0.999f ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
    float3 tangentX = normalize(cross(up, normal));
    float3 tangentY = cross(normal, tangentX);
    return float3x3(tangentX, tangentY, normal);
}

float Pow4(float x)
{
    float xx = x * x;
    return xx * xx;
}

float Pow5(float x)
{
    const float x2 = x * x;
    return x2 * x2 * x;
}

// [Walter et al. 2007, "Microfacet models for refraction through rough surfaces"]
float D_GGX(float NoH, float a)
{
    float d = (NoH * a - NoH) * NoH + 1.f;
    return a / (PI * d * d);
}

// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
float V_SmithGGXCorrelated(float NoV, float NoL, float a)
{
    float ggxL = NoV * sqrt((-NoL * a + NoL) * NoL + a);
    float ggxV = NoL * sqrt((-NoV * a + NoV) * NoV + a);
    return 0.5f / (ggxV + ggxL);
}

float3 ImportanceSampleGGX(float2 E, float rough4)
{
    float Phi = 2.f * PI * E.x;
    float CosTheta = sqrt((1.f - E.y) / (1.f + (rough4 - 1.f) * E.y));
    float SinTheta = sqrt(1.f - CosTheta * CosTheta);

    return float3(SinTheta * cos(Phi), SinTheta * sin(Phi), CosTheta);
}

float2 IntegrateBRDF(float NoV, float roughness)
{
    float a4 = max(Pow4(roughness), 0.00001f); // avoid numeric instability for low roughness
    float3 V;
    V.x = sqrt(1.f - NoV * NoV); // sin
    V.y = 0.f;
    V.z = NoV; // cos
    float A = 0.f;
    float B = 0.f;
    uint NumSamples = Constants.SampleCount;

    for (uint i = 0; i < NumSamples; ++i)
    {
        float2 Xi = Hammersley(i, NumSamples);
        float3 H = ImportanceSampleGGX(Xi, a4);
        float3 L = reflect(-V, H);

        float NoL = saturate(L.z);
        float NoH = saturate(H.z);
        float VoH = saturate(dot(V, H));

        if (NoL > 0.f)
        {
            float G = V_SmithGGXCorrelated(NoV, NoL, a4);
            float G_Vis = 4 * NoL * G * VoH / NoH;
            float Fc = Pow5(1.f - VoH);
            A += (1.f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    return float2(A, B) / NumSamples;
}

float3 PrefilterEnvMap(float roughness, float3 N)
{
    float a4 = Pow4(roughness);
    float3 V = N;

    float3 PrefilteredColor = 0;
    float TotalWeight = 0;
    uint NumSamples = Constants.SampleCount;
    float resolution = Constants.CubeMapInSize;
    float invOmegaP = (6.f * resolution * resolution) * PI * 0.25f;
    float mipLevel = 0;
    float3x3 tangentFrame = GetTangentFrame(N);

    for (uint i = 0; i < NumSamples; ++i)
    {
        float2 Xi = Hammersley(i, NumSamples);
        float3 H = mul(ImportanceSampleGGX(Xi, a4), tangentFrame);
        float3 L = reflect(-V, H);
        float NoL = saturate(dot(N, L));

        if (NoL > 0)
        {
            // [Lagerde et al. 2014, "Moving Frostbite to Physically Based Rendering"]

            float NoH = saturate(dot(N, H));
            float HoV = saturate(dot(H, V));
            float PDF = D_GGX(NoH, a4) * 0.25f;
            float omegaS = 1.f / (float(NumSamples) * PDF + 0.0001f);

            mipLevel = roughness == 0.f ? 0.f : 0.5f * log2(omegaS * invOmegaP);

            PrefilteredColor += TextureCube( ResourceDescriptorHeap[Constants.CubemapsSrvIndex]).SampleLevel(LinearSampler, L, mipLevel).rgb * NoL;
            TotalWeight += NoL;
        }
    }

    return PrefilteredColor / TotalWeight;
}

float3 SampleHemisphereBrute(float3 normal)
{
    float3 irradiance = 0.f;
    float sampleCount = 0.f;
    float invDim = 1.f / Constants.CubeMapInSize;

    for (uint face = 0; face < 6; ++face)
    {
        for (uint y = 0; y < Constants.CubeMapInSize; ++y)
        {
            for (uint x = 0; x < Constants.CubeMapInSize; ++x)
            {
                float2 uv = (float2(x, y) + SAMPLE_OFFSET) * invDim;
                float2 pos = 2.f * uv - 1.f;

                float3 sampleDir = GetSampleDirCubemap(face, pos.x, pos.y);
                float cosTheta = dot(sampleDir, normal);

                if (cosTheta > 0.f)
                {
                    float tmp = 1.f + pos.x * pos.x + pos.y * pos.y;
                    float weight = 4.f * cosTheta / (sqrt(tmp) * tmp);
                    irradiance += TextureCube( ResourceDescriptorHeap[Constants.CubemapsSrvIndex]).SampleLevel(LinearSampler, sampleDir, 0).rgb * weight;
                    sampleCount += weight;
                }
            }
        }
    }

    irradiance *= 1.f / sampleCount;
    return irradiance;
}

float3 ImportanceSampleHemisphere(float3 normal)
{
    float3 irradiance = 0.f;
    const float3x3 tangentFrame = GetTangentFrame(normal);
    const uint sampleCount = Constants.SampleCount;

    for (uint i = 0; i < sampleCount; ++i)
    {
        float2 Xi = Hammersley(i, sampleCount);
        float phi = 2.f * PI * Xi.x; // phi = 2 * PI * u
        float sinTheta = sqrt(Xi.y); // theta = asin(sqrt(v))
        float cosTheta = sqrt(1.f - Xi.y);
        float sinPhi = sin(phi);
        float cosPhi = cos(phi);

        float3 transform = float3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
        float3 sampleDir = mul(transform, tangentFrame);

        irradiance += TextureCube(ResourceDescriptorHeap[Constants.CubemapsSrvIndex]).SampleLevel(LinearSampler, sampleDir, 0).rgb;
    }

    irradiance *= (1.f / float(sampleCount));
    return irradiance;
}

[numthreads(16, 16, 1)]
void EquirectangularToCubeMapCS(uint3 DispatchThreadID : SV_DispatchThreadID, uint3 GroupID : SV_GroupID)
{
    uint face = GroupID.z;
    uint size = Constants.CubeMapOutSize;
    if (DispatchThreadID.x >= size || DispatchThreadID.y >= size || face >= 6)
        return;

    float2 uv = (float2(DispatchThreadID.xy) + SAMPLE_OFFSET) / size;
    float2 pos = 2.f * uv - 1.f;
    float3 sampleDir = GetSampleDirEquirectangular(face, pos.x, pos.y);
    float2 dir = DirectionEquirectangularUV(sampleDir);

    if (Constants.SampleCount == 1)
        dir.x = 1.f - dir.x; // mirror the cubemap
    float4 envMapSample = Texture2D(ResourceDescriptorHeap[Constants.EnvMapSrvIndex]).SampleLevel(LinearSampler, dir, 0);

    const uint pixelIdx = DispatchThreadID.x + DispatchThreadID.y * size;
    const uint faceOffset = face * size * size;
    const uint elIdx = (pixelIdx + faceOffset);
    Output[elIdx] = envMapSample;
}

[numthreads(16, 16, 1)]
void ComputeBRDFIntegrationLUTCS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    uint size = Constants.CubeMapOutSize;
    if (DispatchThreadID.x >= size || DispatchThreadID.y >= size)
        return;

    float2 uv = float2(DispatchThreadID.xy) / (size - 1);
    float2 result = IntegrateBRDF(uv.x, uv.y);

    Output[DispatchThreadID.x + DispatchThreadID.y] = float4(result, 1.f, 1.f);
}

[numthreads(16, 16, 1)]
void PrefilterDiffuseEnvMapCS(uint3 DispatchThreadID : SV_DispatchThreadID, uint3 GroupID : SV_GroupID)
{
    uint face = GroupID.z;
    uint size = Constants.CubeMapOutSize;
    if (DispatchThreadID.x >= size || DispatchThreadID.y >= size || face >= 6)
        return;

    float2 uv = (float2(DispatchThreadID.xy) + SAMPLE_OFFSET) / size;
    float2 pos = 2.f * uv - 1.f;
    float3 sampleDir = GetSampleDirCubemap(face, pos.x, pos.y);
    //float3 irradiance = ImportanceSampleHemisphere(sampleDir);
    float3 irradiance = SampleHemisphereBrute(sampleDir);

    const uint pixelIdx = DispatchThreadID.x + DispatchThreadID.y * size;
    const uint faceOffset = face * size * size;
    Output[pixelIdx + faceOffset] = float4(irradiance, 1.f);
}

[numthreads(16, 16, 1)]
void PrefilterSpecularEnvMapCS(uint3 DispatchThreadID : SV_DispatchThreadID, uint3 GroupID : SV_GroupID)
{
    uint face = GroupID.z;
    uint size = Constants.CubeMapOutSize;
    if (DispatchThreadID.x >= size || DispatchThreadID.y >= size || face >= 6)
        return;

    float2 uv = (float2(DispatchThreadID.xy) + SAMPLE_OFFSET) / size;
    float2 pos = 2.f * uv - 1.f;
    float3 sampleDir = GetSampleDirCubemap(face, pos.x, pos.y);
    float3 irradiance = PrefilterEnvMap(Constants.Roughness, sampleDir);

    const uint pixelIdx = DispatchThreadID.x + DispatchThreadID.y * size;
    const uint faceOffset = face * size * size;
    Output[Constants.OutOffset + pixelIdx + faceOffset] = float4(irradiance, 1.f);
}