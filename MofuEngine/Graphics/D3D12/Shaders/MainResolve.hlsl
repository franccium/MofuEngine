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

// Sclick Fresnel
float3 F_Schlick(float3 F0, float VoH)
{
    float u = Pow5(1.f - VoH);
    float3 F90Approx = saturate(50.f * F0.g);
    return F90Approx * u + (1.f - u) * F0;
}

float3 ApplyReflections(float3 directLighting, float2 uv, float depth, float ao)
{
    const float3 radiance = Sample(ShaderParams.ReflectionsBufferIndex, LinearSampler, uv).rgb;
    const float4 nr = Sample(ShaderParams.NormalBufferIndex, LinearSampler, uv).rgba;
    const float4 mat = Sample(ShaderParams.MaterialPropertiesBufferIndex, LinearSampler, uv).rgba;
    
    const float3 pos_WS = ScreenSpacePosTo3DPos(uv, depth, GlobalData.InvViewProjection).xyz;
    const float3 viewDir = normalize(GlobalData.CameraPosition - pos_WS);
    const float3 n = nr.rgb;
    const float perceptualRoughness = sqrt(mat.r);
    const float metallic = mat.g;
    const float3 albedo = float3(nr.a, mat.b, mat.a);
    const float3 F0 = lerp(0.04f, albedo, metallic);
    const float NoV = saturate(dot(n, viewDir));
    
    
    const float3 F90 = max((1.f - perceptualRoughness), F0);
    const float3 F = F0 + (F90 - F0) * Pow5(1.f - NoV);
    
    AmbientLightParameters IBL = GlobalData.AmbientLight;
    const float3 diffuseColor = albedo * (1.f - metallic);
    float3 diffuse = SampleCube(IBL.DiffuseSrvIndex, LinearSampler, n).rgb * diffuseColor * (1.f - F) * ao;
    
    
    float2 brdfLut = Sample(GlobalData.AmbientLight.BrdfLutSrvIndex, LinearSampler, saturate(float2(NoV, 1.f - perceptualRoughness)), 0).rg;
    
    float3 color = diffuse * IBL.Intensity + ShaderParams.ReflectionsStrength * radiance * (F0 * brdfLut.x + brdfLut.y);
    return saturate(color + directLighting);
}

static const uint SectorCount = 32u;
uint bitCount(uint value)
{
    value = value - ((value >> 1u) & 0x55555555u);
    value = (value & 0x33333333u) + ((value >> 2u) & 0x33333333u);
    return ((value + (value >> 4u) & 0xF0F0F0Fu) * 0x1010101u) >> 24u;
}
float scrolledInterleavedGradNoise(int px, int py, uint frame)
{
    frame = frame % 64;
    float x = float(px) + 5.588238f * float(frame);
    float y = float(py) + 5.588238f * float(frame);
    return fmod(52.9829189f * fmod(0.06711056f * float(x) + 0.00583715f * float(y), 1.0f), 1.0f);
}
float interleavedGradNoise(int px, int py)
{
    return fmod(52.9829189f * fmod(0.06711056f * float(px) + 0.00583715f * float(py), 1.0f), 1.0f);
}
uint UpdateSectors(float minHorizon, float maxHorizon, uint bitfield)
{
    uint bitStart = uint(minHorizon * float(SectorCount));
    uint horizonAngle = uint(ceil((maxHorizon - minHorizon) * float(SectorCount)));
    uint angleBit = horizonAngle > 0u ? uint(0xFFFFFFFFu >> (SectorCount - horizonAngle)) : 0u;
    uint currBitfield = angleBit << bitStart;
    return bitfield | currBitfield;
}

float VBAO(float2 uv, float depth, float2 screenUV)
{
    Texture2D normalBuffer = ResourceDescriptorHeap[ShaderParams.NormalBufferIndex];
    Texture2D posBuffer = ResourceDescriptorHeap[ShaderParams.PositionBufferIndex];
    
    uint indirect = 0u;
    uint occlusion = 0u;
    
    float2 aspectRatio = float2(GlobalData.RenderSizeY, GlobalData.RenderSizeX) / GlobalData.RenderSizeX;
    
    float visibility = 0.f;
    float2 frontBackHorizon = 0.f.xx;
    
    const float sliceCount = GIParams.SliceCount;
    const float sampleCount = GIParams.SampleCount + 2.f * (1.f - depth * 40.f);
    //const float hitThickness = saturate(GIParams.HitThickness + 0.2f * (1.f - depth * 25.f));
    const float hitThickness = GIParams.HitThickness;
    float sliceRotation = TAU / (sliceCount - 1.f);
    
    const float3 normal_VS = normalize(mul((float3x3) GlobalData.View, normalBuffer[uv].rgb));
    float3 pos_VS = posBuffer[uv].rgb;
    const float3 camera = normalize(-pos_VS);
    
    float sampleScale = (-GIParams.SampleRadius * GlobalData.Projection._11) / depth;
    float sampleOffset = 0.01f;
    float jitter = interleavedGradNoise(int(uv.x), int(uv.y)) - 0.5f;
    
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
            float3 samplePos = posBuffer[sampleUV].rgb;
            float3 sampleNormal = normalBuffer[sampleUV].rgb;
            
            sampleNormal = normalize(mul((float3x3) GlobalData.View, sampleNormal));
            
            const float3 sampleDistance = samplePos - pos_VS;
            float sampleLength = length(sampleDistance);
            float3 sampleHorizon = sampleDistance / sampleLength;
            
            frontBackHorizon.x = dot(sampleHorizon, camera);
            frontBackHorizon.y = dot(normalize(sampleDistance - camera * hitThickness), camera);
            frontBackHorizon = acos(frontBackHorizon);
            frontBackHorizon = saturate((frontBackHorizon + n + HALF_PI) / PI);
            
            indirect = UpdateSectors(frontBackHorizon.x, frontBackHorizon.y, 0u);
            
            occlusion |= indirect;
        }
        visibility += 1.f - float(bitCount(occlusion)) / float(SectorCount);
    }

    visibility /= sliceCount;
    
    return visibility;
}

float4 ApplySSILVB(float2 uv, float depth, float2 screenUV)
{
    Texture2D directLightBuffer = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
    Texture2D normalBuffer = ResourceDescriptorHeap[ShaderParams.NormalBufferIndex];
    Texture2D posBuffer = ResourceDescriptorHeap[ShaderParams.PositionBufferIndex];
    
    uint indirect = 0u;
    uint occlusion = 0u;
    
    float2 aspectRatio = float2(GlobalData.RenderSizeY, GlobalData.RenderSizeX) / GlobalData.RenderSizeX;
    
    float visibility = 0.f;
    float3 lighting = 0.f.xxx;
    float2 frontBackHorizon = 0.f.xx;
    
    const float sliceCount = GIParams.SliceCount;
    const float sampleCount = GIParams.SampleCount;
    const float hitThickness = GIParams.HitThickness;
    float sliceRotation = TAU / (sliceCount - 1.f);
    
    const float3 normal_VS = normalize(mul((float3x3) GlobalData.View, normalBuffer[uv].rgb));
    float3 pos_VS = posBuffer[uv].rgb;
    const float3 camera = normalize(-pos_VS);
    
    float sampleScale = (-GIParams.SampleRadius * GlobalData.Projection._11) / depth;
    float sampleOffset = 0.01f;
    float jitter = interleavedGradNoise(int(uv.x), int(uv.y)) - 0.5f;
    
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
            float3 samplePos = posBuffer[sampleUV].rgb;
            float3 sampleNormal = normalBuffer[sampleUV].rgb;
            float3 sampleDirectLight = directLightBuffer[sampleUV].rgb;
            
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

float hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float noise(float2 uv)
{
    float2 i = floor(uv);
    float2 f = frac(uv);
    
    float2 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
    
    return lerp(lerp(hash(i + float2(0.0f, 0.0f)), hash(i + float2(1.0f, 0.0f)), u.x), 
        lerp(hash(i + float2(0.0f, 1.0f)), hash(i + float2(1.0f, 1.0f)), u.x), u.y);
}

float noise(float t)
{
    return sin(t * 12.9898f) * 0.5f + 0.5f;
}

float3 LensFlare(float2 uv, float depth)
{
    float3 color = 0.f.xxx;
    
    //const float3 lightDir = ShaderParams.SunDirection;
    const float3 lightDir = float3(0.2f, -0.2f, 0.f);
    const float3 camDir = GlobalData.CameraDirection;
    //const float3 sunColor = ShaderParams.SunColor;
    const float3 sunColor = float3(0.9f, 0.7f, 0.6f);
    
    float strength = dot(-lightDir, camDir);
    if (strength < 0.f)
        return 0.f.xxx;
    strength = strength * 0.5f + 0.5f;
        
    float2 sunPos = WorldDirToScreenUV(lightDir, GlobalData.CameraPosition, GlobalData.ViewProjection);
    uv = uv - 0.5f;
    sunPos -= 0.5f;
    sunPos.x *= GlobalData.RenderSizeX / GlobalData.RenderSizeY;
    uv.x *= GlobalData.RenderSizeX / GlobalData.RenderSizeY;
    
    float2 sunDir = uv - sunPos;
    float sunDist = length(sunDir);
    float sunRad = 0.5f;
    float edgeFalloff = 0.005f;
    float sunMask = smoothstep(sunRad + edgeFalloff, sunRad - edgeFalloff, sunDist);
    
    if (depth > 0.f)
    {
        return float3(0.12f, 0.03f, 0.01f) * strength;
    }
    
    
    const float3 haloColor = float3(0.25f, 0.23f, 0.21f);
    const float sunCenterIntensity = 1.8f;
    
    // based on Lens Flare Example by mu6k - https://www.shadertoy.com/view/4sX3Rs
    float3 flareColor = 0.f.xxx;
    {
        float2 main = sunDir;
        float2 uvd = uv * (length(uv));
	
        float ang = atan2(main.x, main.y);
        float dist = length(main);
        dist = pow(dist, 0.1f);
        float n = noise(float2(ang * 16.0f, dist * 32.0f));
	
        float center = 1.0f / (length(sunDir) * 16.0f + 1.0f);
	    
        // ciliary corona
        center += center * (sin(noise(sin(ang * 1.f + sunPos.x) * 0.7f - cos(ang * 1.f + sunPos.y)) * 1.2f) * 0.1f + dist * 0.1f + sunCenterIntensity);
        float s1 = max(0.01f - pow(length(uv + 1.2f * sunPos), 1.9f), 0.0f) * 4.0f;

        float s21 = max(1.0f / (1.0f + 32.0f * pow(length(uvd + 0.8f * sunPos), 2.0f)), 0.0f) * haloColor.x;
        float s22 = max(1.0f / (1.0f + 32.0f * pow(length(uvd + 0.85f * sunPos), 2.0f)), 0.0f) * haloColor.y;
        float s23 = max(1.0f / (1.0f + 32.0f * pow(length(uvd + 0.9f * sunPos), 2.0f)), 0.0f) * haloColor.z;
	
        float2 uvx = lerp(uv, uvd, -0.5f);
	
        float s31 = max(0.01f - pow(length(uvx + 0.4f * sunPos), 2.4f), 0.0f) * 6.0f;
        float s32 = max(0.01f - pow(length(uvx + 0.45f * sunPos), 2.4f), 0.0f) * 5.0f;
        float s33 = max(0.01f - pow(length(uvx + 0.5f * sunPos), 2.4f), 0.0f) * 3.0f;
	
        uvx = lerp(uv, uvd, -0.4f);
	
        float s41 = max(0.01f - pow(length(uvx + 0.2f * sunPos), 5.5f), 0.0f) * 2.0f;
        float s42 = max(0.01f - pow(length(uvx + 0.4f * sunPos), 5.5f), 0.0f) * 2.0f;
        float s43 = max(0.01f - pow(length(uvx + 0.6f * sunPos), 5.5f), 0.0f) * 2.0f;
	
        uvx = lerp(uv, uvd, -0.5f);
	
        float s51 = max(0.01f - pow(length(uvx - 0.3f * sunPos), 1.6f), 0.0f) * 6.0f;
        float s52 = max(0.01f - pow(length(uvx - 0.325f * sunPos), 1.6f), 0.0f) * 3.0f;
        float s53 = max(0.01f - pow(length(uvx - 0.35f * sunPos), 1.6f), 0.0f) * 5.0f;
	
        flareColor.r += s21 + s31 + s41 + s51;
        flareColor.g += s22 + s32 + s42 + s52;
        flareColor.b += s23 + s33 + s43 + s53;
        flareColor = flareColor * 1.3f - (length(uvd) * 0.05f); // vignette
        flareColor += center;
    }
    
    color += max(0.f.xxx, flareColor);
    color *= sunColor;
    
    return color;
}

#if PATHTRACE_MAIN
float4 MainResolvePS(in noperspective float4 Position : SV_Position, in noperspective float2 UV : TEXCOORD) : SV_TARGET0
{
    Texture2D gpassMain = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
    Texture2D rtMain = ResourceDescriptorHeap[ShaderParams.RTBufferIndex];
    float4 gpassColor = float4(gpassMain[Position.xy].rgb, 1.f);
    float4 rtColor = float4(rtMain[Position.xy].rgb, 1.f);
    float4 fin = lerp(gpassColor, rtColor, 1.0f);
    return fin;
}
#else
float4 MainResolvePS(in noperspective float4 Position : SV_Position, in noperspective float2 UV : TEXCOORD) : SV_TARGET0
{
    Texture2D gpassDepth = ResourceDescriptorHeap[ShaderParams.GPassDepthBufferIndex];
    float depth = gpassDepth.SampleLevel(PointSampler, UV, 0).r;
    float3 color;
    float3 indirect = 0.f.xxx;
    
    if (depth > 0.0f)
    {
        Texture2D gpassMain = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
        color = gpassMain[Position.xy].rgb;
        
        float ao = 1.f;
        if (ShaderParams.VBAO_Enabled)
        {
            if (ShaderParams.VB_HalfRes)
            {
                Texture2D ssilvb = ResourceDescriptorHeap[ShaderParams.MiscBufferIndex];
#if SSILVB_ONLY_AO
                ao = ssilvb.Sample(LinearSampler, UV).r;
#else
                const float4 visibility = ssilvb.Sample(LinearSampler, UV).rgba;
                indirect = visibility.rgb;
                color = indirect;
                ao = visibility.a;
#endif
            }
            else
            {
#if SSILVB_ONLY_AO
                float vbao = VBAO(Position.xy, depth, UV);
                ao = vbao;
#else
                float4 visibility = ApplySSILVB(Position.xy, depth, UV);
                indirect = visibility.rgb;
                ao = visibility.a;
#endif
            }
        }
        
        if (ShaderParams.SSSR_Enabled)
        {
#if IS_SSSR_ENABLED
        //Texture2D reflections = ResourceDescriptorHeap[ShaderParams.ReflectionsBufferIndex];
            Texture2D normals = ResourceDescriptorHeap[ShaderParams.NormalBufferIndex];
#if 1
        //color = reflections[Position.xy].rgb;
            float3 mainColor = gpassMain[Position.xy].rgb;
            color = ApplyReflections(mainColor, UV, depth, ao);
#else
        //float3 mainColor = gpassMain[Position.xy].rgb;
        float3 mainColor = normals[Position.xy].rgb;
        color = mainColor;
#endif
#endif
        }
        
        color += indirect;
        
        if (ShaderParams.DisplayAO)
        {
            color = ao.xxx;
        }
        
        //Texture2D normals = ResourceDescriptorHeap[ShaderParams.NormalBufferIndex];
        
        //color = float3(texture[Position.xy].xy * 0.5f + 0.5f, 0.f);
#endif
    }
    else
    {
        float4 clip = float4(2.f * UV.x - 1.f, -2.f * UV.y + 1.f, 0.f, 1.f);
        float3 view = mul(GlobalData.InvProjection, clip).xyz;
        float3 direction = mul(view, (float3x3) GlobalData.View); // we swap the order or operations so the view matrix is transposed, since rotation is orthogonal, this makes it inverse view
        color = SampleCube(GlobalData.SkyboxSrvIndex, LinearSampler, direction, 0.1f).xyz * GlobalData.AmbientLight.Intensity;
    }
    
    color += LensFlare(UV, depth);
    color = saturate(color);
    
    
    
    return float4(color, 1.f);
}