
#include "Common.hlsli"

struct GISettings
{
    float SampleCount;
    float SampleRadius;
    float SliceCount;
    float HitThickness;
};

ConstantBuffer<GlobalShaderData> GlobalData : register(b0, space0);
ConstantBuffer<PostProcessConstants> ShaderParams : register(b1, space0);
ConstantBuffer<GISettings> GIParams : register(b2, space0);
SamplerState PointSampler : register(s0, space0);
SamplerState LinearSampler : register(s1, space0);

float4 Sample(uint index, SamplerState s, float2 uv)
{
    return 
    Texture2D( ResourceDescriptorHeap[index]).
    Sample(s, uv);
}

float4 Sample(uint index, SamplerState s, float2 uv, float mip)
{
    return 
    Texture2D( ResourceDescriptorHeap[index]).
    SampleLevel(s, uv, mip);
}

float4 SampleCube(uint index, SamplerState s, float3 n)
{
    return 
    TextureCube( ResourceDescriptorHeap[index]).
    Sample(s, n);
}

float4 SampleCube(uint index, SamplerState s, float3 n, float mip)
{
    return 
    TextureCube( ResourceDescriptorHeap[index]).
    SampleLevel(s, n, mip);
}

static const uint SectorCount = 32u;
uint bitCount(uint value)
{
    value = value - ((value >> 1u) & 0x55555555u);
    value = (value & 0x33333333u) + ((value >> 2u) & 0x33333333u);
    return ((value + (value >> 4u) & 0xF0F0F0Fu) * 0x1010101u) >> 24u;
}
float randf(int x, int y)
{
    return fmod(52.9829189f * fmod(0.06711056f * float(x) + 0.00583715f * float(y), 1.0f), 1.0f);
}
float randf(float x, float y)
{
    return fmod(52.9829189f * fmod(0.06711056f * x + 0.00583715f * y, 1.0f), 1.0f);
}
uint UpdateSectors(float minHorizon, float maxHorizon, uint bitfield)
{
    uint bitStart = uint(minHorizon * float(SectorCount));
    uint horizonAngle = uint(ceil((maxHorizon - minHorizon) * float(SectorCount)));
    uint angleBit = horizonAngle > 0u ? uint(0xFFFFFFFFu >> (SectorCount - horizonAngle)) : 0u;
    uint currBitfield = angleBit << bitStart;
    return bitfield | currBitfield;
}

float4 ApplySSILVB(in noperspective float4 Position : SV_Position, in noperspective float2 halfUV : TEXCOORD) : SV_TARGET0
{
    float2 uv = halfUV * 2.0f;
    const float depth = Sample(ShaderParams.GPassDepthBufferIndex, LinearSampler, uv, 0).r;
    //return float4(depth.xxx * 30.f, 1.f);
    Texture2D directLightBuffer = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
    Texture2D normalBuffer = ResourceDescriptorHeap[ShaderParams.NormalBufferIndex];
    Texture2D posBuffer = ResourceDescriptorHeap[ShaderParams.PositionBufferIndex];
    //return float4(normalBuffer.Sample(LinearSampler, uv, 0).rgb, 1.f);
    //return float4(uv.x, uv.y, 0.f, 1.f);
    uint indirect = 0u;
    uint occlusion = 0u;
    
    float2 aspectRatio = float2(GlobalData.ViewHeight / GlobalData.ViewWidth, 1.f);
    
    float visibility = 0.f;
    float3 lighting = 0.f.xxx;
    float2 frontBackHorizon = 0.f.xx;
    
    const float sliceCount = GIParams.SliceCount;
    const float sampleCount = GIParams.SampleCount;
    const float hitThickness = GIParams.HitThickness;
    float sliceRotation = TAU / (sliceCount - 1.f);
    
    const float3 normal_VS = normalize(mul((float3x3) GlobalData.View, normalBuffer.Sample(LinearSampler, uv).rgb));
    float3 pos_VS = posBuffer.Sample(LinearSampler, uv, 0).rgb;
    const float3 camera = normalize(-pos_VS);
    
    //float sampleScale = (-GIParams.SampleRadius * GlobalData.Projection._11) / depth;
    float sampleScale = (-GIParams.SampleRadius * GlobalData.Projection._11) / -pos_VS.z;
    float sampleOffset = 0.01f;
    float jitter = randf(int(halfUV.x), int(halfUV.y)) - 0.5f;
    
    float currSlice = 0.f;
    for (; currSlice < sliceCount + 0.5f; currSlice += 1.0f)
    {
        float phi = sliceRotation * (currSlice + jitter) + PI;
        float2 omega = float2(cos(phi), sin(phi));
        float3 direction = float3(omega.x, omega.y, 0.f);
        float3 orthoDirection = direction - dot(direction, camera) * camera;
        float3 axis = cross(direction, camera);
        float3 projNormal = normal_VS - axis * dot(normal_VS, axis);
        float projLength = length(projNormal);
        
        float signN = sign(dot(orthoDirection, projNormal));
        float cosN = saturate(dot(projNormal, camera) / projLength);
        float n = signN * acos(cosN);
        
        float currSample = 0.f;
        for (; currSample < sampleCount + 0.5f; currSample += 1.0f)
        {
            const float sampleStep = (currSample + jitter) / sampleCount + sampleOffset;
            
            const float2 sampleUV = uv - sampleStep * sampleScale * omega * aspectRatio;
            float3 samplePos = posBuffer.Sample(LinearSampler, sampleUV, 0).rgb;
            float3 sampleNormal = normalBuffer.Sample(LinearSampler, sampleUV, 0).rgb;
            float3 sampleDirectLight = directLightBuffer.Sample(LinearSampler, sampleUV, 0).rgb;
            
            sampleNormal = normalize(mul((float3x3) GlobalData.View, sampleNormal));
            
            const float3 sampleDistance = samplePos - pos_VS;
            float sampleLength = length(sampleDistance);
            float3 sampleHorizon = sampleDistance / sampleLength;
            
            frontBackHorizon.x = dot(sampleHorizon, camera);
            frontBackHorizon.y = dot(normalize(sampleDistance - camera * hitThickness), camera);
            frontBackHorizon = acos(frontBackHorizon);
            frontBackHorizon = saturate((frontBackHorizon + n + HALF_PI) / PI);
            
            indirect = UpdateSectors(frontBackHorizon.x, frontBackHorizon.y, 0u);
            
            lighting += (1.f - float(bitCount(indirect & ~occlusion)) / float(SectorCount))
                * sampleDirectLight * saturate(dot(normal_VS, sampleHorizon))
                * saturate(dot(sampleNormal, -sampleHorizon));
            occlusion |= indirect;
        }
        visibility += 1.f - float(bitCount(occlusion)) / float(SectorCount);
    }

    visibility /= sliceCount;
    lighting /= sliceCount;
    
    return float4(lighting, visibility);
}
