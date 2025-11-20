#ifndef COMMON_TYPES
#define COMMON_TYPES

struct AmbientLightParameters
{
    float Intensity;
    uint DiffuseSrvIndex;
    uint SpecularSrvIndex;
    uint BrdfLutSrvIndex;
};

struct GlobalShaderData
{
    float4x4 View;
    float4x4 Projection;
    float4x4 InvProjection;
    float4x4 ViewProjection;
    float4x4 InvViewProjection;
    
    float3 CameraPosition;
    float ViewWidth;
    float3 CameraDirection;
    float ViewHeight;
    
    AmbientLightParameters AmbientLight;
    uint SkyboxSrvIndex;
    uint DirectionalLightsCount;
    
#if NEED_MOTION_VECTORS
    float2 DLSSInputResolution;
    float2 Jitter;
    float2 PrevJitter;
#endif
    
    float DeltaTime;
};

struct PerObjectData
{
    float4x4 World;
    float4x4 InvWorld;
    float4x4 WorldViewProjection;
#if NEED_MOTION_VECTORS
    float4x4 PrevWorldViewProjection;
#endif
    
    float4 BaseColor;
    float3 Emissive;
    float EmissiveIntensity;
    float AmbientOcclusion;
    float Metallic;
    float Roughness;
    
    uint16_t MaterialID;
};

struct RTObjectMatrices
{
    float4x4 World;
    float4x4 InvWorld;
};

struct Plane
{
    float3 Normal;
    float Distance; // distance from the origin
};

struct Sphere
{
    float3 Center;
    float Radius;
};

struct Cone
{
    float3 Tip;
    float Height;
    float3 Direction;
    float Radius;
};

struct ConeFrustum
{
    float3 ConeDirection;
    float UnitRadius; // base radius divided by cone length
};

#ifndef __cplusplus
struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID;
    uint3 GroupThreadID : SV_GroupThreadID;
    uint3 DispatchThreadID : SV_DispatchThreadID;
    uint GroupIndex : SV_GroupIndex;
};
#endif

struct LightCullingDispatchParameters
{
    uint2 NumThreadGroups;
    // NOTE: this value can be less than the actual number of threads executed if the screen size is not evenly divisible by the block size
    uint2 NumThreads;
    
    // the number of ligths for culling
    uint NumLights;
    // the index of the current depth buffer in SRV descriptor heap
    uint DepthBufferSrvIndex;
};

// contains light culling data that's formatted and ready to be copied to a D3D constant/structured buffer as a contiguous chunk
struct LightCullingLightInfo
{
    float3 Position;
    float Range;

    float3 Direction;
    float CosPenumbra; // if == -1.f, the light is a point light
};

// light data that's formatted and ready to be copied to a D3D constant/structured buffer as a contigous chunk
// NOTE: in the future we may want to split the light parameter buffer into multiple buffers for each light type
struct CullableLightParameters
{
    float3 Position;
    float Intensity;

    float3 Direction;

    float3 Color;
    float Range;

    float3 Attenuation;

    float CosUmbra;
    float CosPenumbra;
};

struct DirectionalLightParameters
{
    float3 Direction;
    float Intensity;
    float3 Color;
    float _pad;
};

struct GeometryInfo
{
    uint VertexOffset;
    uint IndexOffset;
    uint MaterialIndex;
    uint _pad;
};

struct RayTracingConstants
{
    row_major float4x4 InvViewProjection;
    row_major float4x4 InvView;
    row_major float4x4 InvProj;
    
    float3 SunDirection_WS;
    float CosSunAngularRadius;
    float3 SunIrradiance;
    float SinSunAngularRadius;
    float3 SunColor;
    uint _pad;
    
    float3 CameraPos_WS;
    uint CurrentSampleIndex;
    uint TotalPixelCount;
    
    uint VertexBufferIndex;
    uint IndexBufferIndex;
    uint GeometryInfoBufferIndex;
    uint MaterialBufferIndex;
    uint RTObjectMatricesBufferIndex;
    uint SkyCubemapIndex;
    uint LightCount;
};

struct RTSettings
{
    uint SampleCountSqrt;
    uint MaxPathLength;
    uint MaxAnyHitPathLength;
    
    uint IndirectEnabled;
    uint SpecularEnabled;
    uint DiffuseEnabled;
    uint SunFromDirectionalLight;
    uint RenderSkybox;
    uint ShowNormals;
    uint ShowRayDirs;
    uint SunEnabled;
    uint ShadowsOnly;
    float DiffuseSpecularSelector;
    
    uint BRDFType;
    uint ApplyEnergyConservation;
    uint UseRussianRoulette;
};

#ifdef __cplusplus
static_assert((sizeof(PerObjectData) % 16) == 0, "The PerObjectData struct has to be formatted in 16-byte chunks without any implicit padding.");
static_assert((sizeof(CullableLightParameters) % 16) == 0, "The CullableLightParameters struct has to be formatted in 16-byte chunks without any implicit padding.");
static_assert((sizeof(LightCullingLightInfo) % 16) == 0, "The LightCullingLightInfo struct has to be formatted in 16-byte chunks without any implicit padding.");
static_assert((sizeof(DirectionalLightParameters) % 16) == 0, "The DirectionalLightParameters struct has to be formatted in 16-byte chunks without any implicit padding.");
#endif

#endif