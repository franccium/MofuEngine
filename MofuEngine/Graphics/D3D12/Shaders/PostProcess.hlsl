
#include "Common.hlsli"
#include "PostProcessing/TonemapGT7.hlsl"

#define APPLY_TONEMAPPING 0
#define DO_RESOLVE_PASS 1

#ifdef DEBUG
struct DebugConstants
{
    uint DebugMode; // 0 - default; 1 - depth; 2 - normals; 3 - material IDs
};

#define DEBUG_DEFAULT 0
#define DEBUG_DEPTH 1
#define DEBUG_NORMALS 2
#define DEBUG_MATERIAL_IDS 3
#define DEBUG_MOTION_VECTORS 4
#endif



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
#ifdef DEBUG
ConstantBuffer<DebugConstants> DebugOptions : register(b2, space0);
#endif
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

// sRGB OETF
float3 LinearToSRGB(float3 x)
{
    return select((x <= 0.0031308f), x * 12.92f, 1.055f * pow(x, 0.416666666f) - 0.055f);
}
// PQ inverse EOTF
float3 LinearToPQ(float3 lin)
{
    const float m1 = 0.1593017578125f;
    const float m2 = 78.84375f * EXPONENT_SCALE_FACTOR;
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;
    const float pqC = 10000.0f;

    float3 physical = FrameBufferValueToPhysical(lin);
    float3 y = physical / pqC;
    float3 ym = pow(y, m1);
    float3 num = c1 * c2 * ym;
    float3 den = 1.f + c3 * ym;
    return exp2(m2 * (log2(num) - log2(den)));
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
    
    float3 color = diffuse * IBL.Intensity + radiance * (F0 * brdfLut.x + brdfLut.y);
    return color + directLighting;
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

float VBAO(float2 uv, float depth)
{
    Texture2D normalBuffer = ResourceDescriptorHeap[ShaderParams.NormalBufferIndex];
    Texture2D posBuffer = ResourceDescriptorHeap[ShaderParams.PositionBufferIndex];
    
    uint indirect = 0u;
    uint occlusion = 0u;
    
    float2 aspectRatio = float2(GlobalData.ViewHeight, GlobalData.ViewWidth) / GlobalData.ViewWidth;
    
    float visibility = 0.f;
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

float4 ApplySSILVB(float2 uv, float depth)
{
    Texture2D directLightBuffer = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
    Texture2D normalBuffer = ResourceDescriptorHeap[ShaderParams.NormalBufferIndex];
    Texture2D posBuffer = ResourceDescriptorHeap[ShaderParams.PositionBufferIndex];
    
    uint indirect = 0u;
    uint occlusion = 0u;
    
    float2 aspectRatio = float2(GlobalData.ViewHeight, GlobalData.ViewWidth) / GlobalData.ViewWidth;
    
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
    for (;currSlice < sliceCount + 0.5f; currSlice += 1.0f)
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

#define RENDER_SKYBOX 1
// testing for materials that don't write to the depth buffer, assumes there is a big opaque cube with materialID == 0, but won't work cause that cube would need to be skybox textured anyways
#define MATERIAL_SKYBOX_TEST 0

#if PATHTRACE_MAIN
float4 PostProcessPS(in noperspective float4 Position : SV_Position, in noperspective float2 UV : TEXCOORD) : SV_TARGET0
{
    Texture2D gpassMain = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
    Texture2D rtMain = ResourceDescriptorHeap[ShaderParams.RTBufferIndex];
    float4 gpassColor = float4(gpassMain[Position.xy].rgb, 1.f);
    float4 rtColor = float4(rtMain[Position.xy].rgb, 1.f);
    float4 fin = lerp(gpassColor, rtColor, 1.0f);
    return fin;
}
#else
float4 PostProcessPS(in noperspective float4 Position : SV_Position, in noperspective float2 UV : TEXCOORD) : SV_TARGET0
{
#if DO_RESOLVE_PASS
    
#if IS_DLSS_ENABLED
    Texture2D mainColor = ResourceDescriptorHeap[ShaderParams.MiscBufferIndex];
    //float3 color = mainColor[Position.xy].rgb;
    float3 color = Sample(ShaderParams.MiscBufferIndex, LinearSampler, UV).rgb;

#else
    Texture2D mainColor = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
    float3 color = mainColor[Position.xy].rgb;
    
#endif
        
    if(ShaderParams.DoTonemap)
    {
        color = ApplyTonemap(color);
#if HDR
        color = LinearToPQ(color);
#else
        color = LinearToSRGB(color);
#endif
    }
    
    if (!ShaderParams.RenderGUI)
    {
        color = pow(color, 2.2f);
    }
    
    return float4(color, 1.f);
#else
    
    Texture2D gpassDepth = ResourceDescriptorHeap[ShaderParams.GPassDepthBufferIndex];
    float3 color;
    
#if IS_DLSS_ENABLED
    const float2 targetRes = float2(GlobalData.ViewWidth, GlobalData.ViewHeight);
    const float2 renderRes = GlobalData.DLSSInputResolution;
    const float2 uvScale = renderRes / targetRes;
    const float2 uvScaled = UV * uvScale;
    const float depth = gpassDepth.SampleLevel(PointSampler, UV, 0).r;
#else
    float depth = gpassDepth.SampleLevel(PointSampler, UV, 0).r;
#endif

    
#if RENDER_SKYBOX
#if MATERIAL_SKYBOX_TEST
    Texture2D miscBufer = ResourceDescriptorHeap[ShaderParams.MiscBufferIndex];
    const uint16_t mat = miscBufer[Position.xy].r;
    if (mat > 0)
#else
    if (depth > 0.0f)
#endif
    {
#ifdef DEBUG
        Texture2D texture;
        float3 color;
        if (DebugOptions.DebugMode == DEBUG_DEFAULT)
        {
            texture = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
            color = texture[Position.xy].rgb;
        }
        else if (DebugOptions.DebugMode == DEBUG_DEPTH)
        {
            color = gpassDepth[Position.xy].rrr * 150.f;
        }
        else if (DebugOptions.DebugMode == DEBUG_NORMALS)
        {
            texture = ResourceDescriptorHeap[ShaderParams.NormalBufferIndex];
            color = texture[Position.xy].rgb * 0.5f + 0.5f;
        }
        else if (DebugOptions.DebugMode == DEBUG_MATERIAL_IDS)
        {
            texture = ResourceDescriptorHeap[ShaderParams.NormalBufferIndex];
            color = texture[Position.xy].aaa * 2.f;
        }
        else if (DebugOptions.DebugMode == DEBUG_MOTION_VECTORS)
        {
            texture = ResourceDescriptorHeap[ShaderParams.MotionVectorsBufferIndex];
            color = float3(texture[Position.xy].xy * 0.5f + 0.5f, 0.f);
        }
        color = saturate(color);
#else
        //Texture2D gpassMain = ResourceDescriptorHeap[ShaderParams.MotionVectorsBufferIndex];
#if IS_DLSS_ENABLED
        Texture2D gpassMain = ResourceDescriptorHeap[ShaderParams.RTBufferIndex];
#else
        Texture2D gpassMain = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
#endif
        color = gpassMain[Position.xy].rgb;
        
        float ao = 1.f;
        if (ShaderParams.VBAO_Enabled)
        {
            if (ShaderParams.VB_HalfRes)
            {
                Texture2D ssilvb = ResourceDescriptorHeap[ShaderParams.MotionVectorsBufferIndex];
                float2 lowresUV = UV * 0.5f;
                const float4 visibility = ssilvb.Sample(LinearSampler, lowresUV).rgba;
                //color = visibility.rgb;
                if (ShaderParams.DisplayAO)
                {
                    color = visibility.aaa;
                }
                else
                {
                    ao = visibility.a;
                }
            }
            else
            {
                //float4 visibility = ApplySSILVB(Position.xy, depth);
                //color = visibility.aaa;
                float vbao = VBAO(Position.xy, depth);
                ao = vbao;
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
        color = TextureCube( ResourceDescriptorHeap[GlobalData.SkyboxSrvIndex]).SampleLevel(LinearSampler, direction, 0.1f).xyz * GlobalData.AmbientLight.Intensity;
    }
#else
    Texture2D gpassMain = ResourceDescriptorHeap[ShaderParams.RTBufferIndex];
    color = gpassMain[Position.xy].rgb;
#endif
    
    if(ShaderParams.DoTonemap)
    {
        color = ApplyTonemap(color);
#if HDR
        color = LinearToPQ(color);
#else
        color = LinearToSRGB(color);
#endif
    }
    
    if (!ShaderParams.RenderGUI)
    {
        color = pow(color, 2.2f);
    }
    
    return float4(color, 1.f);
#endif
}
#endif