
#include "Common.hlsli"
#include "Sampling.hlsli"
#include "BRDF.hlsli"

#define SINGLE_MERGED_MODEL 1

enum RayType
{
    RayType_Radiance = 0,
    RayType_Shadow = 1,
    RayType_Count
};

struct RTVertex
{
    float3 Position;
    uint ColorTSign; // rgb - color, z - tangent and normal signs
    float2 UV;
    uint16_t2 Normal;
    uint16_t2 Tangent;
};

struct HitSurface
{
    float3 Position;
    float3 Normal;
    float2 UV;
    float3 Tangent;
    float3 Bitangent;
};

#if SINGLE_MERGED_MODEL
RaytracingAccelerationStructure Scene : register(t0, space121);
#else
RaytracingAccelerationStructure Scene[] : register(t0, space121);
#endif

RWTexture2D<float4> MainOutput : register(u0, space0);

ConstantBuffer<RayTracingConstants> RTConstants : register(b0, space0);
ConstantBuffer<RTSettings> Settings : register(b1, space0);

SamplerState AnisotropicSampler : register(s0, space0);
SamplerState LinearSampler : register(s1, space0);

// : read([]) : write([]) : read+write([])
struct [raypayload] RaygenPayload
{
    float3 Radiance : read(caller) : write(closesthit, miss, caller);
float Roughness : read(caller) : write(caller);
uint PathLength : read(caller, miss, closesthit) : write(caller);
uint PixelIdx : read(caller) : write(caller);
uint SampleSetIdx : read(caller) : write(caller);
bool IsDiffuse : read(caller) : write(caller);
};

struct [raypayload] ShadowPayload
{
    float Visibility : read(caller) : write(closesthit, miss);
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

float3 Fresnel(in float3 specAlbedo, in float3 h, in float3 l)
{
    float3 fresnel = specAlbedo + (1.0f - specAlbedo) * pow((1.0f - saturate(dot(l, h))), 5.0f);

    // Fade out spec entirely when lower than 0.1% albedo
    fresnel *= saturate(dot(specAlbedo, 333.0f));

    return fresnel;
}

//-------------------------------------------------------------------------------------------------
// Calculates the Fresnel factor using Schlick's approximation
//-------------------------------------------------------------------------------------------------
float3 Fresnel(in float3 specAlbedo, in float3 fresnelAlbedo, in float3 h, in float3 l)
{
    float3 fresnel = specAlbedo + (fresnelAlbedo - specAlbedo) * pow((1.0f - saturate(dot(l, h))), 5.0f);

    // Fade out spec entirely when lower than 0.1% albedo
    fresnel *= saturate(dot(specAlbedo, 333.0f));

    return fresnel;
}




//-------------------------------------------------------------------------------------------------
// Helper for computing the Beckmann geometry term
//-------------------------------------------------------------------------------------------------
float BeckmannG1(float m, float nDotX)
{
    float nDotX2 = nDotX * nDotX;
    float tanTheta = sqrt((1 - nDotX2) / nDotX2);
    float a = 1.0f / (m * tanTheta);
    float a2 = a * a;

    float g = 1.0f;
    if (a < 1.6f)
        g *= (3.535f * a + 2.181f * a2) / (1.0f + 2.276f * a + 2.577f * a2);

    return g;
}

//-------------------------------------------------------------------------------------------------
// Computes the specular term using a Beckmann microfacet distribution, with a matching
// geometry factor and visibility term. Based on "Microfacet Models for Refraction Through
// Rough Surfaces" [Walter 07]. m is roughness, n is the surface normal, h is the half vector,
// l is the direction to the light source, and specAlbedo is the RGB specular albedo
//-------------------------------------------------------------------------------------------------
float BeckmannSpecular(in float m, in float3 n, in float3 h, in float3 v, in float3 l)
{
    float nDotH = max(dot(n, h), 0.0001f);
    float nDotL = saturate(dot(n, l));
    float nDotV = max(dot(n, v), 0.0001f);

    float nDotH2 = nDotH * nDotH;
    float nDotH4 = nDotH2 * nDotH2;
    float m2 = m * m;

    // Calculate the distribution term
    float tanTheta2 = (1 - nDotH2) / nDotH2;
    float expTerm = exp(-tanTheta2 / m2);
    float d = expTerm / (PI * m2 * nDotH4);

    // Calculate the matching geometric term
    float g1i = BeckmannG1(m, nDotL);
    float g1o = BeckmannG1(m, nDotV);
    float g = g1i * g1o;

    return d * g * (1.0f / (4.0f * nDotL * nDotV));
}


//-------------------------------------------------------------------------------------------------
// Helper for computing the GGX visibility term
//-------------------------------------------------------------------------------------------------
float GGXV1(in float m2, in float nDotX)
{
    return 1.0f / (nDotX + sqrt(m2 + (1 - m2) * nDotX * nDotX));
}

//-------------------------------------------------------------------------------------------------
// Computes the GGX visibility term
//-------------------------------------------------------------------------------------------------
float GGXVisibility(in float m2, in float nDotL, in float nDotV)
{
    return GGXV1(m2, nDotL) * GGXV1(m2, nDotV);
}

float SmithGGXMasking(float3 n, float3 l, float3 v, float a2)
{
    float dotNL = saturate(dot(n, l));
    float dotNV = saturate(dot(n, v));
    float denomC = sqrt(a2 + (1.0f - a2) * dotNV * dotNV) + dotNV;

    return 2.0f * dotNV / denomC;
}

float SmithGGXMaskingShadowing(float3 n, float3 l, float3 v, float a2)
{
    float dotNL = saturate(dot(n, l));
    float dotNV = saturate(dot(n, v));

    float denomA = dotNV * sqrt(a2 + (1.0f - a2) * dotNL * dotNL);
    float denomB = dotNL * sqrt(a2 + (1.0f - a2) * dotNV * dotNV);

    return 2.0f * dotNL * dotNV / (denomA + denomB);
}

float GGXSpecular(in float m, in float3 n, in float3 h, in float3 v, in float3 l)
{
    float nDotH = saturate(dot(n, h));
    float nDotL = saturate(dot(n, l));
    float nDotV = saturate(dot(n, v));

    float nDotH2 = nDotH * nDotH;
    float m2 = m * m;

    // Calculate the distribution term
    float x = nDotH * nDotH * (m2 - 1) + 1;
    float d = m2 / (PI * x * x);

    // Calculate the matching visibility term
    float vis = GGXVisibility(m2, nDotL, nDotV);

    return d * vis;
}

float GGXSpecularAnisotropic(float m, float3 n, float3 h, float3 v, float3 l, float3 tx, float3 ty, float anisotropy)
{
    float nDotH = saturate(dot(n, h));
    float nDotL = saturate(dot(n, l));
    float nDotV = saturate(dot(n, v));
    float nDotH2 = nDotH * nDotH;

    float ax = m;
    float ay = lerp(ax, 1.0f, anisotropy);
    float ax2 = ax * ax;
    float ay2 = ay * ay;

    float xDotH = dot(tx, h);
    float yDotH = dot(ty, h);
    float xDotH2 = xDotH * xDotH;
    float yDotH2 = yDotH * yDotH;

    // Calculate the distribution term
    float denom = (xDotH2 / ax2) + (yDotH2 / ay2) + nDotH2;
    denom *= denom;
    float d = (1.0f / (PI * ax * ay)) * 1.0f / denom;

    // Calculate the matching visibility term
    float vis = GGXVisibility(m * m, nDotL, nDotV);

    return d * vis;
}

// Distribution term for the velvet BRDF
float VelvetDistribution(in float m, in float nDotH2, in float offset)
{
    m = 0.25f + 0.75f * m;
    float cot2 = nDotH2 / (1.000001f - nDotH2);
    float sin2 = 1.0f - nDotH2;
    float sin4 = sin2 * sin2;
    float amp = 4.0f;
    float m2 = m * m + 0.000001f;
    float cnorm = 1.0f / (PI * (offset + amp * m2));

    return cnorm * (offset + (amp * exp(-cot2 / (m2 + 0.000001f)) / sin4));
}

// Specular term for the velvet BRDF
float VelvetSpecular(in float m, in float3 n, in float3 h, in float3 v, in float3 l, in float offset)
{
    float nDotH = saturate(dot(n, h));
    float nDotH2 = nDotH * nDotH;
    float nDotV = saturate(dot(n, v));
    float nDotL = saturate(dot(n, l));

    float D = VelvetDistribution(m, nDotH2, offset);
    float G = 1.0f;
    float denom = 1.0f / (4.0f * (nDotL + nDotV - nDotL * nDotV));
    return D * G * denom;
}

//-------------------------------------------------------------------------------------------------
// Returns scale and bias values for environment specular reflections that represents the
// integral of the geometry/visibility + fresnel terms for a GGX BRDF given a particular
// viewing angle and roughness value. The final value is computed using polynomials that were
// fitted to tabulated data generated via monte carlo integration.
//-------------------------------------------------------------------------------------------------
float2 GGXEnvironmentBRDFScaleBias(in float nDotV, in float sqrtRoughness)
{
    const float nDotV2 = nDotV * nDotV;
    const float sqrtRoughness2 = sqrtRoughness * sqrtRoughness;
    const float sqrtRoughness3 = sqrtRoughness2 * sqrtRoughness;

    const float delta = 0.991086418474895f + (0.412367709802119f * sqrtRoughness * nDotV2) -
                        (0.363848256078895f * sqrtRoughness2) -
                        (0.758634385642633f * nDotV * sqrtRoughness2);
    const float bias = saturate((0.0306613448029984f * sqrtRoughness) + 0.0238299731830387f /
                                (0.0272458171384516f + sqrtRoughness3 + nDotV2) -
                                0.0454747751719356f);

    const float scale = saturate(delta - bias);
    return float2(scale, bias);
}

//-------------------------------------------------------------------------------------------------
// Returns an adjusted scale factor for environment specular reflections that represents the
// integral of the geometry/visibility + fresnel terms for a GGX BRDF given a particular
// viewing angle and roughness value. The final value is computed using polynomials that were
// fitted to tabulated data generated via monte carlo integration.
//-------------------------------------------------------------------------------------------------
float3 GGXEnvironmentBRDF(in float3 specAlbedo, in float nDotV, in float sqrtRoughness)
{
    float2 scaleBias = GGXEnvironmentBRDFScaleBias(nDotV, sqrtRoughness);
    return specAlbedo * scaleBias.x + scaleBias.y;
}

float BarycentricLerp(in float v0, in float v1, in float v2, in float3 barycentrics)
{
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

float2 BarycentricLerp(in float2 v0, in float2 v1, in float2 v2, in float3 barycentrics)
{
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

float3 BarycentricLerp(in float3 v0, in float3 v1, in float3 v2, in float3 barycentrics)
{
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

float4 BarycentricLerp(in float4 v0, in float4 v1, in float4 v2, in float3 barycentrics)
{
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

const static float InvIntervals = 2.f / ((1 << 16) - 1);
HitSurface GetHitSurfaceFromRTVertex(in RTVertex vtx, in uint geometryIdx)
{
    StructuredBuffer<PerObjectData> perObectDataBuffer = ResourceDescriptorHeap[RTConstants.RTObjectMatricesBufferIndex];
    PerObjectData objectData = perObectDataBuffer[geometryIdx];
    
    HitSurface hitSurface;
    hitSurface.Position = vtx.Position;
    hitSurface.UV = vtx.UV;
    
    uint signs = vtx.ColorTSign >> 24;
    float tSign = float((signs & 0x02) - 1.f); // we get +1.f if set, -1.f if not set
    float handSign = float((signs & 0x01) << 1) - 1.f;
    float nSign = float((signs & 0x04) >> 1) - 1.f;
    uint16_t2 packedNormal = vtx.Normal;
    float2 nXY = vtx.Normal * InvIntervals - 1.f;
    float3 normal = float3(nXY, sqrt(saturate(1.f - dot(nXY, nXY))) * nSign);
    float2 tXY = vtx.Tangent * InvIntervals - 1.f;
    float3 tangent = float3(tXY, sqrt(saturate(1.f - dot(tXY, tXY))) * tSign);
    tangent = tangent - normal * dot(normal, tangent); // use Gram-Schmidt orthogonalization to restore orthogonality
    
    //float3 normal_ws = normalize(mul(normal, (float3x3)objectData.World)).xyz;
    //float4 tangent_ws = float4(normalize(mul(tangent, (float3x3)objectData.World)), handSign);
    
    //hitSurface.Normal = normal_ws;
    hitSurface.Normal = normal;
    //hitSurface.Tangent = tangent_ws.xyz;
    hitSurface.Tangent = tangent;
    //hitSurface.Bitangent = cross(normal_s, tangent_s.xyz) * handSign;
    hitSurface.Bitangent = cross(normal, tangent) * handSign;
    return hitSurface;
}

HitSurface BarycentricLerp(in HitSurface v0, in HitSurface v1, in HitSurface v2, in float3 barycentrics)
{
    HitSurface vtx;
    
    vtx.Position = BarycentricLerp(v0.Position, v1.Position, v2.Position, barycentrics);
    vtx.Normal = normalize(BarycentricLerp(v0.Normal, v1.Normal, v2.Normal, barycentrics));
    vtx.UV = BarycentricLerp(v0.UV, v1.UV, v2.UV, barycentrics);
    vtx.Tangent = normalize(BarycentricLerp(v0.Tangent, v1.Tangent, v2.Tangent, barycentrics));
    vtx.Bitangent = normalize(BarycentricLerp(v0.Bitangent, v1.Bitangent, v2.Bitangent, barycentrics));

    return vtx;
}

float3 CalculateLighting(in float3 positionWS, in float3 normal, in float3 lightDir, in float3 peakIrradiance,
                    in float3 diffuseAlbedo, in float3 specularAlbedo, in float roughness,
                    in float3 cameraPosWS, in float3 msEnergyCompensation)
{
    //if (Settings.BRDFType == 0)
    //{
        float3 lighting = diffuseAlbedo * (1.0f / 3.14159f);
        const float nDotL = saturate(dot(normal, lightDir));
        const float3 view = normalize(cameraPosWS - positionWS);
        if (nDotL > 0.0f)
        {
            const float nDotV = saturate(dot(normal, view));
            float3 h = normalize(view + lightDir);

            float3 fresnel = Fresnel(specularAlbedo, h, lightDir);

            float specular = GGXSpecular(roughness, normal, h, view, lightDir);
            lighting += specular * fresnel * msEnergyCompensation;
        }

    return lighting * nDotL * peakIrradiance;
    //}
    //return 0.0.xxx;
}

static float2 SamplePoint(in uint pixelIdx, inout uint setIdx)
{
    const uint permutation = setIdx * RTConstants.TotalPixelCount + pixelIdx;
    setIdx += 1;
    return SampleCMJ2D(RTConstants.CurrentSampleIndex, Settings.SampleCountSqrt, Settings.SampleCountSqrt, permutation);
}

[shader("raygeneration")]
void RayGen()
{
    const uint2 pxCoord = DispatchRaysIndex().xy;
    const uint pxIdx = pxCoord.y * DispatchRaysDimensions().x + pxCoord.x;
    uint sampleSetIdx = 0;
    
    // primary ray formed by unprojecting pixel coordinates with InvViewProjection
    float2 primaryRaySample = SamplePoint(pxIdx, sampleSetIdx);
    float2 rayPixelPos = pxCoord + primaryRaySample;
    float2 ndcXY = (rayPixelPos / (DispatchRaysDimensions().xy * 0.5f)) - 1.f;
    ndcXY.y *= -1.0f;
    //inverse depth
    float4 rayStart = mul(float4(ndcXY, 1.f, 1.f), RTConstants.InvViewProjection);
    float4 rayEnd = mul(float4(ndcXY, 0.f, 1.f), RTConstants.InvViewProjection);
    rayStart.xyz /= rayStart.w;
    rayEnd.xyz /= rayEnd.w;
    
    const float3 rayDiff = rayEnd.xyz - rayStart.xyz;
    float3 rayDir = normalize(rayDiff);
    float rayLength = length(rayDiff);
    
    RayDesc ray;
    ray.Origin = rayStart.xyz;
    ray.TMin = 0.0f;
    ray.Direction = rayDir;
    ray.TMax = rayLength;
    
    RaygenPayload payload;
    payload.Radiance = float3(0.6, 0.3, 0.1);
    payload.Roughness = 0.5f;
    payload.PathLength = 1;
    payload.PixelIdx = pxIdx;
    payload.SampleSetIdx = sampleSetIdx;
    payload.IsDiffuse = false;
    
    uint rayTraceFlags = RAY_FLAG_NONE;
    if (payload.PathLength > Settings.MaxAnyHitPathLength)
        rayTraceFlags = RAY_FLAG_FORCE_OPAQUE;
    
    const uint hitGroupOffset = RayType_Radiance;
    const uint hitGroupGeometryContMultiplier = RayType_Count;
    const uint missShaderIndex = RayType_Radiance;
    TraceRay(Scene, rayTraceFlags, 0xFFFFFFFF, hitGroupOffset, hitGroupGeometryContMultiplier, missShaderIndex, ray, payload);
    
    payload.Radiance = clamp(payload.Radiance, 0.0f, F16_MAX);
    // satisfying qualifiers for payload members so it compiles
    bool r = payload.Roughness;
    bool d = payload.IsDiffuse;
    uint p = payload.PathLength;
    uint s = payload.SampleSetIdx;
    uint p2 = payload.PixelIdx;
    
    // accumulate radiance
    const float t = RTConstants.CurrentSampleIndex / (RTConstants.CurrentSampleIndex + 1.f);
    float3 sample = payload.Radiance;
    float3 current = MainOutput[pxCoord].xyz;
    float3 accum = lerp(sample, current, t);
    
    if(Settings.ShowRayDirs)
    {
        //MainOutput[pxCoord] = float4(rayPixelPos.xy / DispatchRaysDimensions().xy, 0.f, 1.f);
        MainOutput[pxCoord] = float4(rayDir * 0.5f + 0.5f, 1.f);
    }
    else
    {
        MainOutput[pxCoord] = float4(accum, 1.f);
    }
    //float3 sample = payload.Radiance;
    //MainOutput[pxCoord] = float4(sample, 1.f);
}

static float3 DoPathTracing(in HitSurface hitSurface, in Surface surface, in RaygenPayload inPayload)
{
    if(inPayload.PathLength > 1 && !Settings.IndirectEnabled)
        return 0.0.xxx;
    
    const float3 position_WS = hitSurface.Position;
    const float3 normal_WS = hitSurface.Normal;
    const float3 incomingRayOrigin_WS = WorldRayOrigin();
    const float3 incomingRayDirection_WS = WorldRayDirection();
    
    float3x3 tangentToWorld = float3x3(hitSurface.Tangent, hitSurface.Bitangent, hitSurface.Normal);
    
    float3 baseColor = 1.f;
    float sqrtRoughness = 0.707f;
    float roughness = sqrtRoughness * sqrtRoughness;
    const float3 diffuseAlbedo = float3(1.f, 1.f, 1.f);
    const float3 specularAlbedo = float3(1.f, 1.f, 1.f);
    
    inPayload.Roughness = roughness;
    inPayload.IsDiffuse = false;
    inPayload.PathLength = inPayload.PathLength;
    inPayload.SampleSetIdx = inPayload.SampleSetIdx;
    inPayload.PixelIdx = inPayload.PixelIdx;
    
    float3 msEnergyCompensation = 1.0.xxx;
    if (Settings.ApplyEnergyConservation)
    {
        // apply energy conservation
        // Practical multiple scattering compensation for microfacet models
        const float Ess = GGXEnvironmentBRDFScaleBias(saturate(dot(normal_WS, -incomingRayDirection_WS)), sqrtRoughness).x;
        msEnergyCompensation = 1.0.xxx + specularAlbedo * (1.0f / Ess - 1.0f);
    }
    
    float3 sunDirection_WS = -RTConstants.SunDirection_WS;
    float3 radiance = 0.0.xxx;
    
    if(Settings.SunEnabled)
    {
        RayDesc sunOcclusionRay;
        sunOcclusionRay.Origin = position_WS;
        sunOcclusionRay.TMin = 0.00001f;
        sunOcclusionRay.Direction = sunDirection_WS;
        sunOcclusionRay.TMax = F32_MAX;
    
        uint rayTraceFlags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
        if (inPayload.PathLength > Settings.MaxAnyHitPathLength)
            rayTraceFlags = RAY_FLAG_FORCE_OPAQUE;
    
        ShadowPayload shadowPayload;
    
        const uint hitGroupOffset = RayType_Shadow;
        const uint hitGroupGeometryContMultiplier = RayType_Count;
        const uint missShaderIndex = RayType_Shadow;
        TraceRay(Scene, rayTraceFlags, 0xFFFFFFFF, hitGroupOffset, hitGroupGeometryContMultiplier, missShaderIndex, sunOcclusionRay, shadowPayload);
    
        radiance = shadowPayload.Visibility * CalculateLighting(position_WS, normal_WS, RTConstants.SunDirection_WS,
            RTConstants.SunIrradiance, diffuseAlbedo, specularAlbedo, roughness, incomingRayDirection_WS, msEnergyCompensation);
        
        if (Settings.ShadowsOnly)
        {
            radiance = lerp(radiance, shadowPayload.Visibility, 0.85f);
            return radiance;
        }
    }
    
    // choose the next path with BRDF importance sampling
    float2 brdfSample = SamplePoint(inPayload.PixelIdx, inPayload.SampleSetIdx);
    float3 throughput = 0.f;
    float3 rayDirection_TS = 0.f;
    bool isDiffuse = (brdfSample.x < Settings.DiffuseSpecularSelector);
    if (!Settings.SpecularEnabled)
        isDiffuse = true;
    else if (!Settings.DiffuseEnabled)
        isDiffuse = false;
    if (isDiffuse)
    {
        // sample diffuse - cosine-weighted hemisphere
        if(Settings.SpecularEnabled)
            brdfSample.x *= 2.f;
        rayDirection_TS = SampleDirectionCosineHemisphere(brdfSample.x, brdfSample.y);
        // PDF = NoL / PI, so the terms are canceled out with the diffuse BRDF and irradiance integral
        throughput = diffuseAlbedo;
    }
    else
    {
        // sample specular - GGX BRDF, sampling the distribution of visible normals
        // https://schuttejoe.github.io/post/ggximportancesamplingpart2/
        if(Settings.DiffuseEnabled)
            brdfSample.x = (brdfSample.x - 0.5f) * 2.0f;
        float3 incomingRayDirTS = normalize(mul(incomingRayDirection_WS, transpose(tangentToWorld)));
        float3 microfacetNormalTS = SampleGGXVisibleNormal(-incomingRayDirTS, roughness, roughness, brdfSample.x, brdfSample.y);
        float3 sampleDirTS = reflect(incomingRayDirTS, microfacetNormalTS);

        float3 normalTS = float3(0.0f, 0.0f, 1.0f);

        float3 F = Fresnel(specularAlbedo, microfacetNormalTS, sampleDirTS);
        float G1 = SmithGGXMasking(normalTS, sampleDirTS, -incomingRayDirTS, roughness * roughness);
        float G2 = SmithGGXMaskingShadowing(normalTS, sampleDirTS, -incomingRayDirTS, roughness * roughness);

        throughput = (F * (G2 / G1));
        rayDirection_TS = sampleDirTS;
        
        if(Settings.ApplyEnergyConservation)
        {
            // apply energy conservation
            // Practical multiple scattering compensation for microfacet models
            float Ess = GGXEnvironmentBRDFScaleBias(saturate(dot(normalTS, -incomingRayDirection_WS)), sqrtRoughness).x;
            throughput *= 1.0.xxx + specularAlbedo * (1.0f / Ess - 1.0f);
        }
    }
    
    const float3 rayDirection_WS = normalize(mul(rayDirection_TS, tangentToWorld));
    //const float3 rayDirection_WS = float3(0.5f, 0.5f, 0.5f);
    throughput *= 2.0f;
    
    // get the next path
    RayDesc ray;
    ray.Origin = position_WS;
    ray.TMin = 0.00001f;
    ray.Direction = rayDirection_WS;
    ray.TMax = F32_MAX;
    
    if(Settings.IndirectEnabled && (inPayload.PathLength + 1 < Settings.MaxPathLength))
    {
        RaygenPayload payload;
        payload.Radiance = 0.0f;
        payload.PathLength = inPayload.PathLength + 1;
        payload.PixelIdx = inPayload.PixelIdx;
        payload.SampleSetIdx = inPayload.SampleSetIdx;
        payload.IsDiffuse = isDiffuse;
        payload.Roughness = roughness;

        uint rayTraceFlags = RAY_FLAG_NONE;
        if (payload.PathLength > Settings.MaxAnyHitPathLength)
            rayTraceFlags = RAY_FLAG_FORCE_OPAQUE;
    
        const uint hitGroupOffset = RayType_Radiance;
        const uint hitGroupGeometryContMultiplier = RayType_Count;
        const uint missShaderIndex = RayType_Radiance;
        TraceRay(Scene, rayTraceFlags, 0xFFFFFFFF, hitGroupOffset, hitGroupGeometryContMultiplier, missShaderIndex, ray, payload);
        
        // satisfying qualifiers for payload members so it compiles
        float3 r = payload.Roughness;
        bool d = payload.IsDiffuse;
        uint p = payload.PathLength;
        uint s = payload.SampleSetIdx;
        uint p2 = payload.PixelIdx;

        radiance += payload.Radiance * throughput + (0.1f * payload.IsDiffuse);
    }
    else
    {
        ShadowPayload shadowPayload;

        uint rayTraceFlags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
        
        if (inPayload.PathLength + 1 > Settings.MaxAnyHitPathLength)
            rayTraceFlags = RAY_FLAG_FORCE_OPAQUE;
        
        const uint hitGroupOffset = RayType_Shadow;
        const uint hitGroupGeometryContMultiplier = RayType_Count;
        const uint missShaderIndex = RayType_Shadow;
        TraceRay(Scene, rayTraceFlags, 0xFFFFFFFF, hitGroupOffset, hitGroupGeometryContMultiplier, missShaderIndex, ray, shadowPayload);
        
        TextureCube skyCubemap = ResourceDescriptorHeap[RTConstants.SkyCubemapIndex];
        float3 skyRadiance = skyCubemap.SampleLevel(LinearSampler, rayDirection_WS, 0.0f).xyz;
        radiance += shadowPayload.Visibility * skyRadiance * throughput;
        
        if (Settings.ShadowsOnly)
        {
            radiance = lerp(radiance, shadowPayload.Visibility, 0.85f);
            return radiance;
        }
    }
    

    
    return radiance;
}

// interpolates vertex attributes using barycentric coordinates
HitSurface GetHitSurface(in BuiltInTriangleIntersectionAttributes attr, in uint geometryIdx)
{
    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

    StructuredBuffer<GeometryInfo> geoInfoBuffer = ResourceDescriptorHeap[RTConstants.GeometryInfoBufferIndex];
    const GeometryInfo geoInfo = geoInfoBuffer[geometryIdx];

    StructuredBuffer<RTVertex> vtxBuffer = ResourceDescriptorHeap[RTConstants.VertexBufferIndex];
    Buffer<uint16_t> idxBuffer = ResourceDescriptorHeap[RTConstants.IndexBufferIndex];

    const uint primIdx = PrimitiveIndex();
    const uint idx0 = idxBuffer[primIdx * 3 + geoInfo.IndexOffset + 0];
    const uint idx1 = idxBuffer[primIdx * 3 + geoInfo.IndexOffset + 1];
    const uint idx2 = idxBuffer[primIdx * 3 + geoInfo.IndexOffset + 2];

    const RTVertex vtx0 = vtxBuffer[idx0 + geoInfo.VertexOffset];
    const RTVertex vtx1 = vtxBuffer[idx1 + geoInfo.VertexOffset];
    const RTVertex vtx2 = vtxBuffer[idx2 + geoInfo.VertexOffset];
   // return GetHitSurfaceFromRTVertex(vtx2, geometryIdx);
    return BarycentricLerp(GetHitSurfaceFromRTVertex(vtx0, geometryIdx), GetHitSurfaceFromRTVertex(vtx1, geometryIdx), 
        GetHitSurfaceFromRTVertex(vtx2, geometryIdx), barycentrics);
}

Surface GetGeometryMaterial(in uint geometryIdx)
{
    StructuredBuffer<GeometryInfo> geoInfoBuffer = ResourceDescriptorHeap[RTConstants.GeometryInfoBufferIndex];
    const GeometryInfo geoInfo = geoInfoBuffer[geometryIdx];

    StructuredBuffer<Surface> surfaceBuffer = ResourceDescriptorHeap[RTConstants.MaterialBufferIndex];
    return surfaceBuffer[geoInfo.MaterialIndex];
}

[shader("miss")]
void Miss(inout RaygenPayload payload)
{
    payload.Radiance = float3(0.1f, 0.1f, 0.7f);
    //payload.Radiance = float3(normalize(WorldRayDirection()) * 0.5f + 0.5f);
    
    const float3 rayDirection = WorldRayDirection();
    
    if(Settings.RenderSkybox)
    {
        TextureCube skyCubemap = ResourceDescriptorHeap[RTConstants.SkyCubemapIndex];
        payload.Radiance = skyCubemap.SampleLevel(LinearSampler, rayDirection, 0.0f).xyz;
        if (payload.PathLength == 1)
        {
            float cosSunAngle = dot(rayDirection, -RTConstants.SunDirection_WS);
            if (cosSunAngle >= RTConstants.CosSunAngularRadius)
                payload.Radiance = RTConstants.SunColor;
        }
    }
}

[shader("miss")]
void Shadow_Miss(inout ShadowPayload payload)
{
    payload.Visibility = 1.f;
}

[shader("anyhit")]
void AnyHit(inout RaygenPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    //payload.Radiance = float3(0.0f, 1.0f, 0.0f);
    
    //const HitSurface hitSurface = GetHitSurface(attr, GeometryIndex());
    //const Surface surface = GetGeometryMaterial(GeometryIndex());
    
    //TODO: alpha test
}

[shader("anyhit")]
void Shadow_AnyHit(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
   // const HitSurface hitSurface = GetHitSurface(attr, GeometryIndex());
    //const Surface surface = GetGeometryMaterial(GeometryIndex());
    
    //TODO: alpha test
}

[shader("closesthit")]
void ClosestHit(inout RaygenPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    const HitSurface hitSurface = GetHitSurface(attr, GeometryIndex());
    const Surface surface = GetGeometryMaterial(GeometryIndex());
    /*
    Surface surface; //TODO:
    surface.BaseColor = float3(1.f, 1.f, 1.f);
    surface.Metallic = 0.2f;
    surface.Normal = float3(1.f, 1.f, 1.f);
    surface.PerceptualRoughness = 0.f;
    surface.EmissiveColor = float3(1.f, 1.f, 1.f);
    surface.EmissiveIntensity = 0.f;
    surface.V = float3(1.f, 1.f, 1.f); // view direction
    surface.AmbientOcclusion = 0.f;
    surface.DiffuseColor = float3(1.f, 1.f, 1.f);
    surface.a2 = 0.f;; // Pow(PerceptualRoughness, 2)
    surface.SpecularColor = float3(1.f, 1.f, 1.f);
    surface.NoV = 0.f;;
    surface.SpecularStrength = 0.f;*/
    float3 colors[12] =
    {
        float3(1.f, 0.f, 0.f), float3(0.f, 1.f, 0.f), float3(0.f, 0.f, 1.f), float3(1.f, 1.f, 0.f),
        float3(1.f, 0.f, 1.f), float3(0.f, 1.f, 1.f), float3(1.f, 1.f, 1.f), float3(0.5f, 0.5f, 0.5f),
        float3(0.5f, 0.f, 0.f), float3(0.f, 0.5f, 0.f), float3(0.f, 0.f, 0.5f), float3(0.7f, 0.5f, 0.2f)
    };
    //payload.Radiance = colors[GeometryIndex()];
    payload.Radiance = DoPathTracing(hitSurface, surface, payload);
    if(Settings.ShowNormals)
    {
        payload.Radiance = hitSurface.Normal * 0.5f + 0.5f;
        //payload.Radiance = hitSurface.Normal;
        //payload.Radiance = hitSurface.Tangent;
        //payload.Radiance = hitSurface.Bitangent;
    }
    
    //payload.Radiance = Settings.MaxPathLength / 5.f;
}

[shader("closesthit")]
void Shadow_ClosestHit(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.Visibility = 0.f;
}