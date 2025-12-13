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

uint RandU(uint v)
{
    v = v * v;
    v ^= (v << 13);
    v ^= (v >> 17);
    v ^= (v << 5);
    return v;
}

float RandF_01(inout uint seed)
{
    seed = RandU(seed);
    return float(seed) * (1.0f / 4294967296.0f);
}

float RandF_01_constSeed(uint seed)
{
    seed = RandU(seed);
    return float(seed) * (1.0f / 4294967296.0f);
}

float RandF_neg11(inout uint seed)
{
    seed = RandU(seed);
    return 2.f * float(seed) * (1.0f / 4294967296.0f) - 1.f;
}

float RandF_neg11_constSeed(uint seed)
{
    seed = RandU(seed);
    return 2.f * float(seed) * (1.0f / 4294967296.0f) - 1.f;
}

float3 RandF3_01(inout uint seed)
{
    seed = RandU(seed);
    return float3(RandF_01(seed), RandF_01(seed), RandF_01(seed));
}

float3 RandF3_neg11(inout uint seed)
{
    seed = RandU(seed);
    return float3(RandF_neg11(seed), RandF_neg11(seed), RandF_neg11(seed));
}

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

// saves a color sample and its depth to the transparency list and increases the head's item count
// the caller is responsible for making sure the sample position fits in the buffer
void SaveTransparencyEntry(uint2 screenSize, uint2 screenPos, float4 color, float depth)
{
    if(color.a <= 0.f) return;
    
    uint bufferIdx = screenPos.y * screenSize.x + screenPos.x;
    
    uint listIdx;
    InterlockedAdd(TransparencyListHeadBuffer[bufferIdx], 1, listIdx);
    if(listIdx > MAX_TRANSPARENCY_LAYERS) return;
    
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
    
    uint x = threadID.x;
    uint y = threadID.x;
    x = clamp(x, 0, GlobalData.RenderSizeX);
    y = clamp(y, 0, GlobalData.RenderSizeY - 2);

    uint flags = FlagsBuffer[particleIdx];
    // return if its not initialized in the flags buffer - doesn't belong to any set
    if (flags == 0) return;
    
    uint setIdx = flags & PARTICLE_SET_IDX_MASK;
    ParticleSet set = ParticleSetBuffer[setIdx];
    const float maxSetParticleCount = ceil(set.ParticlesSpawnedPerSecond * set.InitialAge);
    if (particleIdx - set.ParticleStartIndex > maxSetParticleCount)
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
    
    float3 position;
    float4 velocityAge;
    UnpackParticleData(set, packedData, position, velocityAge);
    float3 velocity = velocityAge.xyz;
    float age = velocityAge.w;
    
    uint seed = SimulationData.Seed * particleIdx;
    
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
            velocity = float3(0.8f, 0.8f, 0.8f) * RandF3_neg11(seed);
            
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
    //velocity = float3(0.8f, 0.8f, 0.8f) * RandF3_neg11(seed);
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
    
    // store the particle's final data
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

    const float3 vertexPos_VS = vertexPos_CS.xyz / vertexPos_CS.w;
    float2 vertexPos_uv = vertexPos_VS.xy;
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
        
        minMaxX = float2(min(minMaxX.x, quadVtx[vtx].x), max(minMaxX.y, quadVtx[vtx].x));
        minMaxY = float2(min(minMaxY.x, quadVtx[vtx].y), max(minMaxY.y, quadVtx[vtx].y));
    }
    
    if (minMaxX.y < 0.0f || minMaxX.x > 1.0f || minMaxY.y < 0.0f || minMaxY.x > 1.0f)
        return;
    
    const float2 screenSize = float2(GlobalData.RenderSizeX, GlobalData.RenderSizeY);
    const float2 vertexScreenPos = vertexPos_uv * screenSize;

    //const uint2 u_minMaxX = clamp(floor(minMaxX * screenSize.x), 0, floor(screenSize.x - 1.f));
    //const uint2 u_minMaxY = clamp(floor(minMaxY * screenSize.y), 0, floor(screenSize.y - 1.f));

    minMaxX = clamp(minMaxX * screenSize.x, 0.f, screenSize.x - 1.f);
    minMaxY = clamp(minMaxY * screenSize.y, 0.f, screenSize.y - 1.f);
    // particles which are too big should be sent to the hardware rasterizer
    if ((minMaxX.y - minMaxX.x) * (minMaxY.y - minMaxY.x) > PARTICLES_HW_RASTERIZATION_THRESHOLD)
    {
        //TODO:
        return;
    }

    if (minMaxX.y - minMaxX.x <= 1.0f || minMaxY.y - minMaxY.x <= 1.0f)
    {
        uint x = uint(minMaxX.x);
        uint y = uint(minMaxY.y);
        
        float4 color = float4(0.f, 0.f, 1.f, 1.0f);
        
        SaveTransparencyEntry(uint2(screenSize.x, screenSize.y), uint2(x, y), color, vertexPos_VS.z);
    }
    else
    {
        for (uint y = uint(minMaxY.x); y < uint(minMaxY.y); y++)
        {
            for (uint x = uint(minMaxX.x); x < uint(minMaxX.y); x++)
            {
                float4 color = float4(0.9f, 0.3f, 0.3f, 0.7f);
                
                SaveTransparencyEntry(uint2(screenSize.x, screenSize.y), uint2(x, y), color, vertexPos_VS.z);
            }
        }
    }
}