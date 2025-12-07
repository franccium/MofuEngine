#include "Common.hlsli"
#include "ParticlesTypes.hlsli"
#include "DataPacking.hlsli"
// Particle System based on The Forge's particle system

ConstantBuffer<GlobalShaderData> GlobalData;
ConstantBuffer<ParticleBufferState> BufferState;

RWStructuredBuffer<uint> FlagsBuffer;
RWStructuredBuffer<ParticleSet> ParticleSetBuffer;
RWByteAddressBuffer ParticleDataBuffer;

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
    pos.xyz /= PARTICLE_MAX_SET_SCALE;
    velocityAge.xyz /= set.MaxVelocity;
    
    ParticleDataPacked packed;
    packed.Position = F3NormtoU2FixedPoint(pos);
    packed.VelocityAge = uint2(Pack2xFloat1(velocityAge.x, velocityAge.y), Pack2xFloat1(velocityAge.z, velocityAge.w / set.InitialAge));
    return packed;
}

void StoreParticle(ParticleSet set, uint particleIdx, float flags, float3 pos, float age, float3 velocity, float animTime)
{
    ParticleDataPacked data = PackParticleData(set, pos, float4(velocity, age));
    data.AnimTime = animTime;
    SetParticleData(data, ParticleDataBuffer, particleIdx);
    FlagsBuffer[particleIdx] = flags;
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
    
    return true;
}

// each thread simulates a single particle
[numthreads(PARTICLE_BATCH_SIZE_X, PARTICLE_BATCH_SIZE_Y, 1)]
void ParticleSimulationCS(ComputeShaderInput csIn)
{
    const uint groupCountInDimension = uint(ceil(sqrt(MAX_PARTICLE_COUNT / (PARTICLE_BATCH_SIZE_X * PARTICLE_BATCH_SIZE_Y))));
    const uint particleIdx = csIn.DispatchThreadID.y * PARTICLE_BATCH_SIZE_X * groupCountInDimension + csIn.DispatchThreadID.x;
    if (particleIdx >= MAX_PARTICLE_COUNT) return;
    
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
                    StoreParticle(set, particleIdx, flags, float3(0, 0, 0), age, float3(0, 0, 0), animTime);
                    return;
                }
                
                particleBufferOffset += setParticleCount;
            }
        }
    }
#endif
    
    uint flags = FlagsBuffer[particleIdx];
    if(flags == 0) return;
    
    uint setIdx = flags & PARTICLE_SET_IDX_MASK;
    ParticleSet set = ParticleSetBuffer[setIdx];
    if (particleIdx - set.ParticleStartIndex > ceil(set.ParticlesSpawnedPerSecond * set.InitialAge))
    {
        FlagsBuffer[particleIdx] = 0;
        return;
    }
    
    if (flags & PARTICLE_STATE_NEED_INIT)
    {
        
    }
    
    if (!(flags & PARTICLE_STATE_ALLOCATED) || !IsParticleSetVisible(set))
    {
        return;
    }
    
    ParticleDataPacked packedData;
    GetParticleData(packedData, ParticleDataBuffer, particleIdx);
    
    float initialAge;
    float lightRadius;
    float position;
    float3 velocity;
    bool isAlive = flags & PARTICLE_STATE_ALIVE;
    if(!isAlive)
    {
        if(age >= 0.f)
        {
            isAlive = true;
            
            switch (set.ParticleType)
            {
                case PARTICLE_TYPE_1:
                    initialAge = set.InitialAge;
                    age = initialAge;
                    lightRadius = set.LightRadius;
                    position = set.Position;
                    velocity = float3(0.5f, 0.f, 0.f);
                    flags |= PARTICLE_STATE_MOVING;
                    break;
                
                case PARTICLE_TYPE_2:
                    initialAge = set.InitialAge;
                    age = initialAge;
                    lightRadius = set.LightRadius;
                    position = set.Position;
                    velocity = float3(0.3f, 0.7f, 0.f);
                    flags |= PARTICLE_STATE_MOVING;
                    break;
                
                case PARTICLE_TYPE_3:
                    initialAge = set.InitialAge;
                    age = initialAge;
                    lightRadius = set.LightRadius;
                    position = set.Position;
                    velocity = float3(0.1f, 0.3f, 0.3f);
                    flags |= PARTICLE_STATE_MOVING;
                    break;
            }
            
            flags |= PARTICLE_STATE_ALIVE | set.ParticleType | set.ParticleLightMode;
            StoreParticle(set, particleIdx, flags, position, age, velocity, animTime);
        }
        else
        {
            age -= GlobalData.DeltaTime;
            StoreParticle(set, particleIdx, flags, position, age, velocity, animTime);
        }
    }
    else if (age < 0)
    {
        // the lifetime is over
        flags &= ~PARTICLE_STATE_ALIVE;
        // we are setting negative age, the particle should respawn when age > 0
        age = -max(0.f, float(setParticleCount) / float(setParticlesPerSecond) - initialAge);
        StoreParticle(set, particleIdx, flags, position, age, velocity, animTime);
    }
}