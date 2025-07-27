#include "Common.hlsli"

// NOTE: should be larger than MAX_LIGHTS_PER_TILE (max for the average number of lights per tile), this is the maximum number of lights per tile
static const uint MAX_LIGHTS_PER_GROUP = 1024;

 // the tile's minimum and maximum depth in view space
groupshared uint _minDepthVS;
groupshared uint _maxDepthVS;

// the number of lights that affect pixels in this tile
groupshared uint _lightCount;
// offset in the global light index list where we copy _lightIndexList
groupshared uint _lightIndexStartOffset;
// indices of lights which affect this tile
groupshared uint _lightIndexList[MAX_LIGHTS_PER_GROUP];
// flags the lights that actually contribute to this tile's lighting, == 2 for spot lights and == 1 for point lights
groupshared uint _workingLights_Opaque[MAX_LIGHTS_PER_GROUP];
groupshared uint _spotLightStartOffset;
// x - points lights, y - spot lights
groupshared uint2 _opaqueLightIndex;

ConstantBuffer<GlobalShaderData> GlobalData : register(b0, space0);
ConstantBuffer<LightCullingDispatchParameters> DispatchParams : register(b1, space0);
StructuredBuffer<ConeFrustum> Frustums : register(t0, space0);
StructuredBuffer<LightCullingLightInfo> Lights : register(t1, space0);
StructuredBuffer<Sphere> BoundingSpheres : register(t2, space0);

RWStructuredBuffer<uint> LightIndexCounter : register(u0, space0);
RWStructuredBuffer<uint2> LightGrid_Opaque : register(u1, space0);

RWStructuredBuffer<uint> LightIndexList_Opaque : register(u3, space0);

bool TestSphereConeIntersection(Sphere sphere, ConeFrustum cone, float minDepth, float maxDepth)
{
    if ((sphere.Center.z - sphere.Radius > minDepth) || (sphere.Center.z + sphere.Radius < maxDepth))
        return false;
    
    const float3 lightRejection = sphere.Center - dot(sphere.Center, cone.ConeDirection) * cone.ConeDirection;
    const float distSq = dot(lightRejection, lightRejection);
    const float radius = sphere.Center.z * cone.UnitRadius + sphere.Radius;
    const float radiusSq = radius * radius;
    
    return distSq <= radiusSq;
}

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void LightCullingCS(ComputeShaderInput csIn)
{
    const uint gridIndex = csIn.GroupID.x + (csIn.GroupID.y * DispatchParams.NumThreadGroups.x);
    const ConeFrustum frustum = Frustums[gridIndex];
    
    const float depth = Texture2D(ResourceDescriptorHeap[DispatchParams.DepthBufferSrvIndex])[csIn.DispatchThreadID.xy].r;
    const float C = GlobalData.Projection._m22;
    const float D = GlobalData.Projection._m23;
    
    // initialize group data
    if(csIn.GroupIndex == 0)
    {
        _minDepthVS = FLT_MAX;
        _maxDepthVS = 0;
        _lightCount = 0;
        _opaqueLightIndex = 0;
    }
    
    uint i = 0;
    uint index = 0;
    
    for (i = csIn.GroupIndex; i < MAX_LIGHTS_PER_GROUP; i += TILE_SIZE * TILE_SIZE)
    {
        _workingLights_Opaque[i] = 0;
    }
    
    // DEPTH MIN/MAX
    GroupMemoryBarrierWithGroupSync();
    
    // don't include far plane, the depth buffer is reversed, so 0 is mapped to the far plane
    if(depth != 0)
    {
        // swap min/max because of reversed depth
        const float depthMin = WaveActiveMax(depth);
        const float depthMax = WaveActiveMin(depth);
        
        if (WaveIsFirstLane())
        {
            // negate depth because of right-handed coordinates
            const uint zMin = asuint(D / (depthMin + C)); // -minDepthVS as uint
            const uint zMax = asuint(D / (depthMax + C)); // -maxDepthVS as uint
            
            InterlockedMin(_minDepthVS, zMin);
            InterlockedMax(_maxDepthVS, zMax);
        }
    }

    // LIGHT CULLING
    GroupMemoryBarrierWithGroupSync();
    // negate the depth values to end up with negative z values
    const float minDepthVS = -asfloat(_minDepthVS);
    const float maxDepthVS = -asfloat(_maxDepthVS);
    
    for (i = csIn.GroupIndex; i < DispatchParams.NumLights; i += TILE_SIZE * TILE_SIZE)
    {
        Sphere sphere = BoundingSpheres[i];
        sphere.Center = mul(GlobalData.View, float4(sphere.Center, 1.f)).xyz;
        
        if (TestSphereConeIntersection(sphere, frustum, minDepthVS, maxDepthVS))
        {
            InterlockedAdd(_lightCount, 1, index);
            if (index < MAX_LIGHTS_PER_GROUP)
                _lightIndexList[index] = i;
        }
    }
    
    // LIGHT PRUNING
    GroupMemoryBarrierWithGroupSync();
    // TODO: look at other depth discontinuity solutions
    const uint lightCount = min(_lightCount, MAX_LIGHTS_PER_GROUP);
    const float2 invViewDimensions = 1.f / float2(GlobalData.ViewWidth, GlobalData.ViewHeight);
    const float2 uv = csIn.DispatchThreadID.xy * invViewDimensions;
    const float3 pxWorldPos = ScreenSpacePosTo3DPos(uv, depth, GlobalData.InvViewProjection).xyz;
    for (i = 0; i < lightCount; i++)
    {
        index = _lightIndexList[i];
        const LightCullingLightInfo light = Lights[index];
        const float3 distToPx = pxWorldPos - light.Position;
        const float distSq = dot(distToPx, distToPx);
        
        if (distSq <= light.Range * light.Range)
        {
            const bool isPointLight = light.CosPenumbra == -1.f;
            if (isPointLight || (dot(distToPx * rsqrt(distSq), light.Direction) >= light.CosPenumbra))
            {
                // the pixel is within the cone
                _workingLights_Opaque[i] = 2 - uint(isPointLight);
            }
        }
        
        //_workingLights_Opaque[i] = 2 - uint(1);
        
        //const bool isPointLight = light.CosPenumbra < 0.0f;
        //if (isPointLight || light.CosPenumbra > 0.f)
        //{
        //         // the pixel is within the cone
        //    _workingLights_Opaque[i] = 2 - uint(isPointLight);
        //}
    }
    
    // UPDATE LIGHT GRID
    GroupMemoryBarrierWithGroupSync();
    if (csIn.GroupIndex == 0)
    {
        uint numPointLights = 0;
        uint numSpotLights = 0;
        for (i = 0; i < lightCount; ++i)
        {
            // since _workingLights_Opaque[i] == 2 for spot lights and == 1 for point lights
            numPointLights += (_workingLights_Opaque[i] & 1);
            numSpotLights += (_workingLights_Opaque[i] >> 1);
        }
        
        InterlockedAdd(LightIndexCounter[0], numPointLights + numSpotLights, _lightIndexStartOffset);
        _spotLightStartOffset = _lightIndexStartOffset + numPointLights;
        LightGrid_Opaque[gridIndex] = uint2(_lightIndexStartOffset, (numPointLights << 16) | numSpotLights);
    }
    
    // UPDATE LIGHT INDEX LIST
    GroupMemoryBarrierWithGroupSync();
    
    uint pointIndex, spotIndex;
    
    for (i = csIn.GroupIndex; i < lightCount; i += TILE_SIZE * TILE_SIZE)
    {
        if (_workingLights_Opaque[i] == 1)
        {
            InterlockedAdd(_opaqueLightIndex.x, 1, pointIndex);
            LightIndexList_Opaque[_lightIndexStartOffset + pointIndex] = _lightIndexList[i];
        }
        else if (_workingLights_Opaque[i] == 2)
        {
            InterlockedAdd(_opaqueLightIndex.y, 1, spotIndex);
            LightIndexList_Opaque[_spotLightStartOffset + spotIndex] = _lightIndexList[i];
        }
    }
}