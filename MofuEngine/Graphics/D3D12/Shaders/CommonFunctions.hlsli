#ifndef COMMON_FUNCTIONS
#define COMMON_FUNCTIONS

float Pow5(float x)
{
    const float x2 = x * x;
    return x2 * x2 * x;
}

float4 ClipToView(float4 clip, float4x4 inverseProjection)
{
    // view space position
    float4 view = mul(inverseProjection, clip);
    // undo the perspective divide
    return view / view.w;
}

// converts a normalized screen-space position to a 3D coordinate in space depending on the given inverse
// depth - z buffer depth (reversed z)
// inverse:
// inv projection - screen to view space 
// inv view_projection - screen to world space
// inv world_view_projection - screen to object space
float4 ScreenSpacePosTo3DPos(float2 uv, float depth, float4x4 inverse)
{
    // convert to clip space
    float4 clip = float4(float2(uv.x, 1.f - uv.y) * 2.f - 1.f, depth, 1.f);
    
    float4 position = mul(inverse, clip);
    // get the 3D-space position
    return position / position.w;
}

float4 ScreenToView(float4 screen, float2 invViewDimensions, float4x4 inverseProjection)
{
    // convert to normalized texture coordinates
    float2 texCoord = screen.xy * invViewDimensions;
    // convert to clip space
    float4 clip = float4(float2(texCoord.x, 1.f - texCoord.y) * 2.f - 1.f, screen.z, screen.w);
    return ClipToView(clip, inverseProjection);
}

float2 WorldDirToScreenUV(float3 worldDir, float3 camPos, float4x4 viewProjection)
{
    float3 worldPos = camPos + worldDir * 1000.0f;
    float4 clipPos = mul(viewProjection, float4(worldPos, 1.0f));
    float3 ndc = clipPos.xyz / clipPos.w;
    float2 uv = ndc.xy * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;
    return uv;
}

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

#endif