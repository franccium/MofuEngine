#include "Common.hlsli"
#include "ParticlesTypes.hlsli"
#include "DataPacking.hlsli"
// Particle System based on The Forge's particle system

ConstantBuffer<GlobalShaderData> GlobalData : register(b0, space0);
ConstantBuffer<ParticleBufferState> BufferState : register(b1, space0);
ConstantBuffer<ParticleSimulationData> SimulationData : register(b2, space0);

RWStructuredBuffer<uint> FlagsBuffer : register(u0, space0);
RWStructuredBuffer<ParticleSet> ParticleSetBuffer : register(u1, space0);
RWByteAddressBuffer ParticleDataBuffer : register(u2, space0);

RWStructuredBuffer<uint> TransparencyListHeadBuffer : register(u3, space0);
RWStructuredBuffer<ParticleTransparencyNodePacked> TransparencyList : register(u4, space0);

SamplerState PointSampler : register(s0, space0);

#define GetParticleData(res, particlesDataBuffer, idx) \
    res.VelocityAge.x = particlesDataBuffer.Load((PARTICLE_DATA_STRIDE * (idx)) << 2); \
	res.VelocityAge.y = particlesDataBuffer.Load((PARTICLE_DATA_STRIDE * (idx) + 1) << 2); \
	res.Position.x = particlesDataBuffer.Load((PARTICLE_DATA_STRIDE * (idx) + 2) << 2); \
	res.Position.y = particlesDataBuffer.Load((PARTICLE_DATA_STRIDE * (idx) + 3) << 2); \
	res.AnimTime = asfloat(particlesDataBuffer.Load((PARTICLE_DATA_STRIDE * (idx) + 4) << 2))

#define SetParticleData(data, particlesDataBuffer, idx) \
    particlesDataBuffer.Store((PARTICLE_DATA_STRIDE * (idx)) << 2, data.VelocityAge.x); \
	particlesDataBuffer.Store((PARTICLE_DATA_STRIDE * (idx) + 1) << 2, data.VelocityAge.y); \
	particlesDataBuffer.Store((PARTICLE_DATA_STRIDE * (idx) + 2) << 2, data.Position.x); \
	particlesDataBuffer.Store((PARTICLE_DATA_STRIDE * (idx) + 3) << 2, data.Position.y); \
	particlesDataBuffer.Store((PARTICLE_DATA_STRIDE * (idx) + 4) << 2, asuint(data.AnimTime))

ParticleDataPacked PackParticleData(ParticleSet set, float3 pos, float4 velocityAge)
{
    pos.xyz -= set.Position;
    pos.xyz /= PARTICLES_MAX_SET_SCALE;
    
    velocityAge.xyz /= (Unpack2xFloat1(set.MaxSizeAndSpeed).y + 0.01f);
    
    ParticleDataPacked packed;
    packed.Position = F3NormtoU2FixedPoint(pos);
    packed.VelocityAge = uint2(Pack2xFloat1(velocityAge.x, velocityAge.y), Pack2xFloat1(velocityAge.z, velocityAge.w / set.InitialAge));
    return packed;
}

void UnpackParticleData(ParticleSet set, ParticleDataPacked packed, out float3 pos, out float4 velocityAge)
{
    pos = U2FixedPointToF3Norm(packed.Position);
    pos.xyz *= PARTICLES_MAX_SET_SCALE;
    pos.xyz += set.Position;
    
    velocityAge.xy = Unpack2xFloat1(packed.VelocityAge.x);
    velocityAge.zw = Unpack2xFloat1(packed.VelocityAge.y);
    velocityAge.xyz *= (Unpack2xFloat1(set.MaxSizeAndSpeed).y + 0.01f);
    velocityAge.w *= set.InitialAge;
}

void StoreParticle(ParticleSet set, uint particleIdx, float flags, float3 pos, float age, float3 velocity, float animTime)
{
    ParticleDataPacked data = PackParticleData(set, pos, float4(velocity, age));
    data.AnimTime = animTime;
    SetParticleData(data, ParticleDataBuffer, particleIdx);
    FlagsBuffer[particleIdx] = flags;
}

void SaveTransparencyEntry(uint2 screenSize, uint2 screenPos, float4 color, float depth)
{
    if(color.a <= 0.f) return;
    
    uint bufferIdx = screenPos.y * screenSize.x + screenPos.x;
    if(TransparencyListHeadBuffer[bufferIdx] >= MAX_TRANSPARENCY_LAYERS) return;
    uint listIdx;
    
    InterlockedAdd(TransparencyListHeadBuffer[bufferIdx], 1, listIdx);
    if (listIdx >= MAX_TRANSPARENCY_LAYERS) return;
    --listIdx;
    
    ParticleTransparencyNodePacked node;
    node.Depth = depth;
    node.Color = F4NormToUnorm4x8(color);
    
    TransparencyList[bufferIdx * MAX_TRANSPARENCY_LAYERS + listIdx] = node;
}

#ifdef NO_PARTICLE_SET_BUFFER
ParticleSet GetParticleSet(uint setIdx)
{
    
}
#endif

bool IsParticleSetVisible(ParticleSet set)
{
    if (!set.IsAllocated) return false;
    
    float4 position_CS = mul(GlobalData.ViewProjection, float4(set.Position, 1.f));
    float3 position_NDC = position_CS.xyz / position_CS.w;
    
    //TODO:
    
    return true;
}

// each thread simulates a single particle
[numthreads(PARTICLES_BATCH_SIZE_X, PARTICLES_BATCH_SIZE_Y, 1)]
void ParticlesSimulationCS(uint3 threadID : SV_DispatchThreadID)
//void ParticlesSimulationCS(ComputeShaderInput csIn)
{
    const uint groupCountInDimension = uint(ceil(sqrt(MAX_PARTICLE_COUNT / (PARTICLES_BATCH_SIZE_X * PARTICLES_BATCH_SIZE_Y))));
    //const uint particleIdx = csIn.DispatchThreadID.y * PARTICLES_BATCH_SIZE_X * groupCountInDimension + csIn.DispatchThreadID.x;
    const uint particleIdx = threadID.y * PARTICLES_BATCH_SIZE_X * groupCountInDimension + threadID.x;
    if (particleIdx >= MAX_PARTICLE_COUNT)
        return;
    
    
    //uint2 ss = uint2(GlobalData.RenderSizeX, GlobalData.RenderSizeY);
    //float2 asfasdf = float2(30.f, 90.f);
    //float2 awgwgaw = float2(40.f, 100.f);
    //// particles which are too big should be sent to the hardware rasterizer
    //if ((awgwgaw.y - awgwgaw.x) * (asfasdf.y - asfasdf.x) > PARTICLES_HW_RASTERIZATION_THRESHOLD)
    //{
    //    //TODO:
    //}

    //if (awgwgaw.y - awgwgaw.x <= 1.0f || asfasdf.y - asfasdf.x <= 1.0f)
    //{
    //    uint x = uint(asfasdf.y);
    //    uint y = uint(awgwgaw.y);
        
    //    float4 color = float4(1.f, 0.f, 0.f, 1.f);
        
    //    SaveTransparencyEntry(ss, uint2(x, y), color, 0.2f);
    //}
    //else
    //{
    //    for (uint y = uint(awgwgaw.x); y < uint(awgwgaw.y); y++)
    //    {
    //        for (uint x = uint(asfasdf.x); x < uint(asfasdf.y); x++)
    //        {
    //            float4 color = float4(1.f, 0.f, 0.f, 1.f);
                
    //            SaveTransparencyEntry(ss, uint2(x, y), color, 0.2f);
    //        }
    //    }
    //}
    
    
    
    //uint xSize = GlobalData.RenderSizeX - 800;
    //uint ySize = GlobalData.RenderSizeY - 400;
    ////for (uint x = 0; x < 1067 * 600 - 1; x++)
    //for (uint x = 0; x < xSize * ySize - 1; x++)
    //{
    //    TransparencyListHeadBuffer[x] = 1;
        
    //    ParticleTransparencyNodePacked node;
    //    node.Depth = 0.2f;
    //    node.Color = F4NormToUnorm4x8(float4(1.f, 1.f, 0.f, 1.f));
    //    TransparencyList[x * MAX_TRANSPARENCY_LAYERS + 0] = node;
    //}
    //return;
    uint x = threadID.x;
    uint y = threadID.x;
    x = clamp(x, 0, GlobalData.RenderSizeX);
    y = clamp(y, 0, GlobalData.RenderSizeY - 2);
    //const uint bufferII = x;
    ////FlagsBuffer[x] = uint(2);
    //uint2 screenPos = uint2(x, y);
    //uint2 ss = uint2(GlobalData.RenderSizeX, GlobalData.RenderSizeY);
    //uint bufferIdx = screenPos.y * ss.x + screenPos.x;
    //if (bufferIdx > ss.x * ss.y)
    //    return;
    //if (TransparencyListHeadBuffer[bufferIdx] >= MAX_TRANSPARENCY_LAYERS)
    //    return;
    //uint listIdx;
    
    //InterlockedAdd(TransparencyListHeadBuffer[bufferIdx], 1, listIdx);
    //if (listIdx >= MAX_TRANSPARENCY_LAYERS)
    //    return;
    //--listIdx;
    
    //ParticleTransparencyNodePacked node;
    //node.Depth = 0.2f;
    //node.Color = F4NormToUnorm4x8(float4(1.f, 0.f, 0.f, 1.f));
    
    //TransparencyList[bufferIdx * MAX_TRANSPARENCY_LAYERS + listIdx] = node;
    
    //SaveTransparencyEntry(uint2(GlobalData.RenderSizeX, GlobalData.RenderSizeY), uint2(x, y), float4(0.1f, 0.6f, 0.9f, 1.f), 0.00001f);
    
#ifdef NO_PARTICLE_SET_BUFFER
    uint flags = 0;
    float age;
    float animTime;
    const uint lightShadowCount = BufferState.ParticleCount[0];
    const uint lightCount = BufferState.ParticleCount[1];
    const uint noLightCount = BufferState.ParticleCount[2];
    
    // the layout is by light mode: lightShadow | light | none
    uint3 startIndices = uint3(0, lightShadowCount, lightShadowCount + lightCount);
    uint3 endIndices = uint3(lightShadowCount, lightShadowCount + lightCount, lightShadowCount + lightCount + noLightCount);
    uint3 lightModes = uint3(PARTICLE_LIGHT_MODE_LIGHTSHADOW, PARTICLE_LIGHT_MODE_LIGHT, PARTICLE_LIGHT_MODE_NONE);
    
    // allocate particle
    ParticleSet set;
    float setParticleCount;
    float setParticlesPerSecond;
    for (uint light = 0; light < PARTICLE_LIGHT_MODE_COUNT; light++)
    {
        const uint particleBufferOffset = startIndices[light];
        for (uint setIdx = 0; setIdx < MAX_PARTICLE_SET_COUNT; setIdx++)
        {
            set = GetParticleSet(setIdx);
            
            setParticleCount = set.MaxAllocatedParticles;
            setParticlesPerSecond = set.ParticlesSpawnedPerSecond;
            
            if (set.ParticleLightMode == lightModes[light])
            {
                // if the particle is in this set, allocate it
                if (particleIdx >= particleBufferOffset && particleIdx < particleBufferOffset + setParticleCount)
                {
                    flags |= PARTICLE_IS_ALLOCATED | setIdx | lightModes[light] | set.ParticleType;
                    // at the beginning, light will not be visible
                    flags |= PARTICLE_LIGHT_CULLED;
                    
                    age = float(particleIdx - particleBufferOffset) / setParticlesPerSecond;
                    StoreParticle(set, particleIdx, flags, float3(0.f, 0.f, 0.f), age, float3(0.f, 0.f, 0.f), animTime);
                    return;
                }
                
                particleBufferOffset += setParticleCount;
            }
        }
    }
#endif
    
    uint flags = FlagsBuffer[particleIdx];
    if (flags == 0) return;
    
    uint setIdx = flags & PARTICLE_SET_IDX_MASK;
    ParticleSet set = ParticleSetBuffer[setIdx];
    if (particleIdx - set.ParticleStartIndex > ceil(set.ParticlesSpawnedPerSecond * set.InitialAge))
    {
        FlagsBuffer[particleIdx] = 0;
        return;
    }
    
    if (flags & PARTICLE_STATE_NEED_INIT)
    {
        flags = (flags & PARTICLE_SET_IDX_MASK) | PARTICLE_LIGHT_CULLED | PARTICLE_STATE_ALLOCATED | set.SetFlags;
        const float age = float(particleIdx - set.ParticleStartIndex) / -set.ParticlesSpawnedPerSecond;
        StoreParticle(set, particleIdx, flags, float3(0.f, 0.f, 0.f), age, float3(0.f, 0.f, 0.f), 0);
    }
    
    if (!(flags & PARTICLE_STATE_ALLOCATED) || !IsParticleSetVisible(set))
    {
        return;
    }
    
    ParticleDataPacked packedData;
    GetParticleData(packedData, ParticleDataBuffer, particleIdx);
    const float maxSetParticleCount = ceil(set.ParticlesSpawnedPerSecond * set.InitialAge);
    
    float3 position;
    float4 velocityAge;
    UnpackParticleData(set, packedData, position, velocityAge);
    float3 velocity = velocityAge.xyz;
    float age = velocityAge.w;
    
    float initialAge;
    float lightRadius;
    
    bool isAlive = flags & PARTICLE_STATE_ALIVE;
    
    const float maxSpeed = Unpack2xFloat1(set.MaxSizeAndSpeed).y;
    float dt = min(0.066f, GlobalData.DeltaTime);
    age = !isAlive ? (age + dt) : (age - dt);
    
    if (!isAlive)
    {
        if (age >= 0.f)
        {
            isAlive = true;
            age = set.InitialAge - age;
            position = set.Position;
            velocity = float3(0.2f, 0.3f, 0.2f);
            /*
            switch (set.ParticleType)
            {
                case PARTICLE_TYPE_1:
                    initialAge = set.InitialAge;
                    age = initialAge;
                    lightRadius = set.LightRadius;
                    position = set.Position;
                    velocity = float3(0.5f, 0.f, 0.f);
                    flags |= PARTICLE_STATE_ACCELERATING;
                    break;
                
                case PARTICLE_TYPE_2:
                    initialAge = set.InitialAge;
                    age = initialAge;
                    lightRadius = set.LightRadius;
                    position = set.Position;
                    velocity = float3(0.3f, 0.7f, 0.f);
                    flags |= PARTICLE_STATE_ACCELERATING;
                    break;
                
                case PARTICLE_TYPE_3:
                    initialAge = set.InitialAge;
                    age = initialAge;
                    lightRadius = set.LightRadius;
                    position = set.Position;
                    velocity = float3(0.1f, 0.3f, 0.3f);
                    flags |= PARTICLE_STATE_ACCELERATING;
                    break;
            }*/
            
            flags |= PARTICLE_STATE_ALIVE | set.SetFlags;
            StoreParticle(set, particleIdx, flags, position, age, velocity, 0);
        }
        else
        {
            age -= GlobalData.DeltaTime;
            StoreParticle(set, particleIdx, flags, position, age, velocity, 0);
        }
    }
    else if (age < 0)
    {
        // the lifetime is over
        flags = ((flags & ~PARTICLE_STATE_ALIVE) | PARTICLE_LIGHT_CULLED);
        // we are setting negative age, the particle should respawn when age > 0
        //age = -max(0.f, float(setParticleCount) / float(set.ParticlesSpawnedPerSecond) - initialAge);
        age = 0.f;
        StoreParticle(set, particleIdx, flags, position, age, velocity, 0);
    }
    
    //////// Simulate Particle
    position += velocity * dt;
 
    float4 vertexPos = float4(position, 1.f);
    float4 vertexPos_CS = mul(GlobalData.ViewProjection, vertexPos);
    float3 vertexPos_NDC = vertexPos_CS.xyz / vertexPos_CS.w;
    
    float2 uv;
    
    if (flags & PARTICLE_COLLIDE_DEPTHBUFFER)
    {
        uv = vertexPos_NDC.xy * 0.5f + 0.5f;
        uv.y = 1.f - uv.y;
        
        if (uv.x <= 1.f && uv.x >= 0.f && uv.y <= 1.f && uv.y >= 0.f)
        {
            const float particleDepth = vertexPos_NDC.z;
            const float depth = Sample(SimulationData.DepthBufferIndex, PointSampler, uv).r;

            if (particleDepth > depth && abs(particleDepth - depth) < PARTICLES_COLLISION_THRESHOLD)
            {
                flags = ((flags & ~(PARTICLE_STATE_ACCELERATING | PARTICLE_COLLIDE_DEPTHBUFFER | PARTICLE_BILLBOARD_MODE_MASK)) | PARTICLE_COLLIDED);
                position -= velocity * dt;
                velocity = float3(0.f, 0.f, 0.f);
            }
        }
    }
    
    float animTime = packedData.AnimTime;
    
    switch (set.SetFlags & PARTICLE_TYPE_MASK)
    {
        case PARTICLE_TYPE_1:
            velocity += float3(0.f, 0.f, 0.f);
            break;
                
        case PARTICLE_TYPE_2:
            velocity += dt * float3(0.5f, 0.2f, 0.f);
            break;
                
        case PARTICLE_TYPE_3:
            velocity += dt * float3(-0.2f, 0.2f, -0.3f);
            break;
    }
    
    //////// Cull Particle Lights
    flags &= ~PARTICLE_LIGHT_CULLED;
    if (flags & PARTICLE_LIGHT_MODE_LIGHT && !(flags & PARTICLE_LIGHT_MODE_LIGHTSHADOW))
    {
        //TODO: light culling
        //if (0)
        //{
        //    flags |= PARTICLE_LIGHT_CULLED;
        //    StoreParticle(set, particleIdx, flags, position, age, velocity, animTime);
        //    return;
        //}
    }
    
    StoreParticle(set, particleIdx, flags, position, age, velocity, animTime);
    
    //if (animTime >= 1.f) return;
    
    //////// Rasterize Particle
    const float particleScale = 1.f;
    float4 quadVtx[QUAD_VERTEX_COUNT] =
    {
        float4(-particleScale, -particleScale, 0.f, 1.f),
        float4(particleScale, -particleScale, 0.f, 1.f),
        float4(particleScale, particleScale, 0.f, 1.f),
        float4(-particleScale, particleScale, 0.f, 1.f)
    };
    
    vertexPos_NDC = mul(GlobalData.View, vertexPos).xyz;
    velocity = mul(GlobalData.View, float4(velocity, 0.f)).xyz;
    
    float4x4 billboardTransform;
    if (flags & PARTICLE_BILLBOARD_MODE_VELOCITYORIENT)
    {
        const float3 up = normalize(velocity.xyz);
        const float3 right = cross(up, float3(0.f, 0.f, 1.f));
        const float3 forward = cross(up, right);
        billboardTransform[0] = float4(right.x, up.x, forward.x, vertexPos.x);
        billboardTransform[1] = float4(right.y, up.y, forward.y, vertexPos.y);
        billboardTransform[2] = float4(right.z, up.z, forward.z, vertexPos.z);
        billboardTransform[3] = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    
    uint vtx = 0;
    [unroll(QUAD_VERTEX_COUNT)]
    for (vtx = 0; vtx < QUAD_VERTEX_COUNT; vtx++)
    {
        if (flags & PARTICLE_BILLBOARD_MODE_VELOCITYORIENT)
        {
            quadVtx[vtx] = mul(billboardTransform, quadVtx[vtx]);
            quadVtx[vtx] = mul(GlobalData.ViewProjection, quadVtx[vtx]);
        }
        else
        {
            quadVtx[vtx].xyz -= vertexPos_NDC;
            quadVtx[vtx] = mul(GlobalData.Projection, quadVtx[vtx]);
        }
    }

    float2 vertexPos_uv = vertexPos_CS.xy / vertexPos_CS.w;
    vertexPos_uv = vertexPos_uv * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    
    float2 minMaxX;
    float2 minMaxY;
    
    [unroll(QUAD_VERTEX_COUNT)]
    for (vtx = 0; vtx < QUAD_VERTEX_COUNT; vtx++)
    {
        quadVtx[vtx] /= quadVtx[vtx].w;
        quadVtx[vtx].xy = quadVtx[vtx].xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

        if (vtx == 0)
        {
            minMaxX = quadVtx[0].x.xx;
            minMaxY = quadVtx[0].y.xx;
        }
        
        minMaxX = float2(min(minMaxX.x, quadVtx[vtx].x), max(minMaxX.x, quadVtx[vtx].x));
        minMaxY = float2(min(minMaxY.x, quadVtx[vtx].y), max(minMaxY.x, quadVtx[vtx].y));
    }
    
    if (minMaxX.y < 0.0f || minMaxX.x > 1.0f || minMaxY.y < 0.0f || minMaxY.x > 1.0f)
        return;
    
    const float2 screenSize = float2(GlobalData.RenderSizeX, GlobalData.RenderSizeY);
    const float2 vertexScreenPos = vertexPos_uv * screenSize;

    minMaxX = clamp(minMaxX * screenSize.x, 0.f, screenSize.x - 1.f);
    minMaxY = clamp(minMaxY * screenSize.y, 0.f, screenSize.y - 1.f);

    // particles which are too big should be sent to the hardware rasterizer
    if ((minMaxY.y - minMaxY.x) * (minMaxX.y - minMaxX.x) > PARTICLES_HW_RASTERIZATION_THRESHOLD)
    {
        //TODO:
        return;
    }

    if (minMaxY.y - minMaxY.x <= 1.0f || minMaxX.y - minMaxX.x <= 1.0f)
    {
        uint x = uint(minMaxX.y);
        uint y = uint(minMaxY.y);
        
        float4 color = float4(0.f, 0.f, 1.f, 0.5f);
        
        SaveTransparencyEntry(uint2(screenSize.x, screenSize.y), uint2(x, y), color, vertexPos.z);
    }
    else
    {
        for (uint y = uint(minMaxY.x); y < uint(minMaxY.y); y++)
        {
            for (uint x = uint(minMaxX.x); x < uint(minMaxX.y); x++)
            {
                float4 color = float4(0.6f, 0.3f, 0.f, 0.1f);
                
                SaveTransparencyEntry(uint2(screenSize.x, screenSize.y), uint2(x, y), color, vertexPos.z);
            }
        }
    }
}