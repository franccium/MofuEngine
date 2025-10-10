
#include "Common.hlsli"
#include "Sampling.hlsli"

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

RaytracingAccelerationStructure Scene : register(t0, space121);

RWTexture2D<float4> MainOutput : register(u0, space0);

ConstantBuffer<RayTracingConstants> RTConstants : register(b0, space0);

SamplerState AnisotropicSampler : register(s0, space0);
SamplerState LinearSampler : register(s1, space0);

struct RaygenPayload
{
    float3 Radiance;
    float Roughness;
    uint PathLength;
    uint PixelIdx;
    uint SampleSetIdx;
    bool IsDiffuse;
};

struct ShadowPayload
{
    float Visibility;
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

#define SAMPLE_COUNT_SQRT 3 //TODO: get from buffer
#define MAX_ANY_HIT_PATH_LENGTH 1
#define MAX_PATH_LENGTH 3
#define INDIRECT_ENABLED 1

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
HitSurface GetHitSurfaceFromRTVertex(in RTVertex vtx)
{
    HitSurface hitSurface;
    hitSurface.Position = vtx.Position;
    hitSurface.UV = vtx.UV;
    
    uint signs = vtx.ColorTSign >> 24;
    float nSign = float((signs & 0x04) >> 1) - 1.f;
    uint16_t2 packedNormal = vtx.Normal;
    float2 nXY = vtx.Normal * InvIntervals - 1.f;
    float3 normal = float3(nXY, sqrt(saturate(1.f - dot(nXY, nXY))) * nSign);
    
    hitSurface.Normal = normal;
    //hitSurface.Tangent = 
    //TODO:
    hitSurface.Tangent = float3(0.25f, 0.3f, 0.22f);
    hitSurface.Bitangent = float3(0.65f, 0.45f, 0.62f);
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

static float2 SamplePoint(in uint pixelIdx, inout uint setIdx)
{
    const uint permutation = setIdx * RTConstants.TotalPixelCount + pixelIdx;
    setIdx += 1;
    return SampleCMJ2D(RTConstants.CurrentSampleIndex, SAMPLE_COUNT_SQRT, SAMPLE_COUNT_SQRT, permutation);
}

[shader("raygeneration")]
void RayGen()
{
    const uint2 pxCoord = DispatchRaysIndex().xy;
    const uint pxIdx = pxCoord.y * DispatchRaysDimensions().x + pxCoord.x;
    uint sampleSetIdx = 0;
    
    // primary ray fformed by unprojecting pixel coordinates with InvViewProjection
    float2 primaryRaySample = SamplePoint(pxIdx, sampleSetIdx);
    float2 rayPixelPos = pxCoord + primaryRaySample;
    float2 ndcXY = (rayPixelPos / (DispatchRaysDimensions().xy * 0.5f)) - 1.f;
    ndcXY.y *= -1.0f;
    float4 rayStart = mul(float4(ndcXY, 0.f, 1.f), RTConstants.InvViewProjection);
    float4 rayEnd = mul(float4(ndcXY, 1.f, 1.f), RTConstants.InvViewProjection);
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
    payload.Radiance = 0.f;
    payload.Roughness = 0.5f;
    payload.PathLength = 1.f;
    payload.PixelIdx = pxIdx;
    payload.SampleSetIdx = sampleSetIdx;
    payload.IsDiffuse = false;
    
    uint rayTraceFlags = RAY_FLAG_NONE;
    if (payload.PathLength > MAX_ANY_HIT_PATH_LENGTH)
        rayTraceFlags = RAY_FLAG_FORCE_OPAQUE;
    
    const uint hitGroupOffset = RayType_Radiance;
    const uint hitGroupGeometryContMultiplier = RayType_Count;
    const uint missShaderIndex = RayType_Radiance;
    TraceRay(Scene, rayTraceFlags, 0xFFFFFFFF, hitGroupOffset, hitGroupGeometryContMultiplier, missShaderIndex, ray, payload);
    
    payload.Radiance = clamp(payload.Radiance, 0.0f, F16_MAX);
    
    // accumulate radiance
    const float t = RTConstants.CurrentSampleIndex / (RTConstants.CurrentSampleIndex + 1.f);
    float3 sample = payload.Radiance;
    float3 current = MainOutput[pxCoord].xyz;
    float3 accum = lerp(sample, current, t);
    
    MainOutput[pxCoord] = float4(accum, 1.f);
    //float3 sample = payload.Radiance;
    //MainOutput[pxCoord] = float4(sample, 1.f);
}

static float3 DoPathTracing(in HitSurface hitSurface, in Surface surface, in RaygenPayload inPayload)
{
    if(inPayload.PathLength > 1 && !INDIRECT_ENABLED)
        return 0.0.xxx;
    
    return float3(1.f, 1.f, 1.f);
}

// interpolates vertex attributes using barycentric coordinates
HitSurface GetHitSurface(in BuiltInTriangleIntersectionAttributes attr, in uint geometryIdx)
{
    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

    StructuredBuffer<GeometryInfo> geoInfoBuffer = ResourceDescriptorHeap[RTConstants.GeometryInfoBufferIndex];
    const GeometryInfo geoInfo = geoInfoBuffer[geometryIdx];

    StructuredBuffer<RTVertex> vtxBuffer = ResourceDescriptorHeap[RTConstants.VertexBufferIndex];
    Buffer<uint> idxBuffer = ResourceDescriptorHeap[RTConstants.IndexBufferIndex];

    const uint primIdx = PrimitiveIndex();
    const uint idx0 = idxBuffer[primIdx * 3 + geoInfo.IndexOffset + 0];
    const uint idx1 = idxBuffer[primIdx * 3 + geoInfo.IndexOffset + 1];
    const uint idx2 = idxBuffer[primIdx * 3 + geoInfo.IndexOffset + 2];

    const RTVertex vtx0 = vtxBuffer[idx0 + geoInfo.VertexOffset];
    const RTVertex vtx1 = vtxBuffer[idx1 + geoInfo.VertexOffset];
    const RTVertex vtx2 = vtxBuffer[idx2 + geoInfo.VertexOffset];

    return BarycentricLerp(GetHitSurfaceFromRTVertex(vtx0), GetHitSurfaceFromRTVertex(vtx1), GetHitSurfaceFromRTVertex(vtx2), barycentrics);
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
    payload.Radiance = float3(0.3f, 0.3f, 0.3f);
    
    /*
    const float3 rayDirection = WorldRayDirection();
    
    TextureCube skyCubemap = ResourceDescriptorHeap[RTConstants.SkyCubemapIndex];
    payload.Radiance = skyCubemap.SampleLevel(LinearSampler, rayDirection, 0.0f).xyz;

    if (payload.PathLength == 1)
    {
        float cosSunAngle = dot(rayDirection, RTConstants.SunDirection_WS);
        if (cosSunAngle >= RTConstants.CosSunAngularRadius)
            payload.Radiance = RTConstants.SunColor;
    }*/
}

[shader("miss")]
void Shadow_Miss(inout ShadowPayload payload)
{
    payload.Visibility = 1.f;
}

[shader("anyhit")]
void AnyHit(inout RaygenPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.Radiance = float3(0.0f, 0.2f, 1.0f);
    
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
    //const HitSurface hitSurface = GetHitSurface(attr, GeometryIndex());
    //const Surface surface = GetGeometryMaterial(GeometryIndex());
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
    payload.Radiance = float3(1.0f, 0.3f, 0.3f);
    //payload.Radiance = DoPathTracing(hitSurface, surface, payload);
}

[shader("closesthit")]
void Shadow_ClosestHit(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.Visibility = 0.f;
}