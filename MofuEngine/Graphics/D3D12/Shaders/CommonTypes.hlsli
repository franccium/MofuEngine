#ifndef COMMON_TYPES
#define COMMON_TYPES

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
    
    float DeltaTime;
    uint3 _pad;
};

struct PerObjectData
{
    float4x4 World;
    float4x4 InvWorld;
    float4x4 WorldViewProjection;
    
    float4 BaseColor;
};

#ifdef __cplusplus
static_assert((sizeof(PerObjectData) % 16) == 0, "The PerObjectData struct has to be formatted in 16-byte chunks without any implicit padding.");
#endif

#endif