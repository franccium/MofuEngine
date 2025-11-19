#include "Sampling.hlsli"
#include "BRDF.hlsli"

#define SINGLE_MERGED_MODEL 1

enum RayType
{
    RayType_Shadow = 0,
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

RWTexture2D<float> VisibilityBuffer : register(u0, space0);

ConstantBuffer<RayTracingConstants> RTConstants : register(b0, space0);
ConstantBuffer<RTSettings> Settings : register(b1, space0);

// : read([]) : write([]) : read+write([])
struct [raypayload] RaygenPayload
{
    uint PathLength : read(caller, miss, closesthit) : write(caller);
    uint PixelIdx : read(caller) : write(caller);
    uint SampleSetIdx : read(caller) : write(caller);
};

struct [raypayload] ShadowPayload
{
    float Visibility : read(caller) : write(closesthit, miss);
};

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
    
    if (Settings.ShowRayDirs)
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
    if (inPayload.PathLength > 1 && !Settings.IndirectEnabled)
        return 0.0.xxx;
    
    const float3 position_WS = hitSurface.Position;
    const float3 normal_WS = hitSurface.Normal;
    const float3 incomingRayOrigin_WS = WorldRayOrigin();
    const float3 incomingRayDirection_WS = WorldRayDirection();
    
    float3x3 tangentToWorld = float3x3(hitSurface.Tangent, hitSurface.Bitangent, hitSurface.Normal);
    
    inPayload.PathLength = inPayload.PathLength;
    inPayload.SampleSetIdx = inPayload.SampleSetIdx;
    inPayload.PixelIdx = inPayload.PixelIdx
    
    float3 sunDirection_WS = -RTConstants.SunDirection_WS;
    
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
    
    
    float3 rayDirection_TS = 0.f;
        rayDirection_TS = SampleDirectionCosineHemisphere(brdfSample.x, brdfSample.y);
    
    const float3 rayDirection_WS = normalize(mul(rayDirection_TS, tangentToWorld));
    
    // get the next path
    RayDesc ray;
    ray.Origin = position_WS;
    ray.TMin = 0.00001f;
    ray.Direction = rayDirection_WS;
    ray.TMax = F32_MAX;

    ShadowPayload shadowPayload;

    uint rayTraceFlags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
        
    if (inPayload.PathLength + 1 > Settings.MaxAnyHitPathLength)
        rayTraceFlags = RAY_FLAG_FORCE_OPAQUE;
        
    const uint hitGroupOffset = RayType_Shadow;
    const uint hitGroupGeometryContMultiplier = RayType_Count;
    const uint missShaderIndex = RayType_Shadow;
    TraceRay(Scene, rayTraceFlags, 0xFFFFFFFF, hitGroupOffset, hitGroupGeometryContMultiplier, missShaderIndex, ray, shadowPayload);
    
    return shadowPayload.Visibility;
}

[shader("miss")]
void Miss(inout RaygenPayload payload)
{
    
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
    
}

[shader("closesthit")]
void Shadow_ClosestHit(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.Visibility = 0.f;
}