#ifndef PARTICLE_TYPES
#define PARTICLE_TYPES

#define MAX_PARTICLE_SET_COUNT 16
#define MAX_PARTICLE_LIGHTSHADOW_COUNT 8
#define MAX_PARTICLE_LIGHT_COUNT 1000
#define MAX_PARTICLE_NOLIGHT_COUNT 4000
#define MAX_PARTICLE_COUNT (MAX_PARTICLE_LIGHTSHADOW_COUNT + MAX_PARTICLE_LIGHT_COUNT + MAX_PARTICLE_NOLIGHT_COUNT)

#define PARTICLES_BATCH_SIZE_X 32
#define PARTICLES_BATCH_SIZE_Y 1

// Particle Flags
// 0x00000FFF --> occupied by set index - up to 4096
#define PARTICLE_SET_IDX_MASK 0xFFFu

// light mode
#define PARTICLE_LIGHT_MODE_COUNT 3u
#define PARTICLE_LIGHT_MODE_MASK 0x6000000u
#define PARTICLE_LIGHT_MODE_NONE 0x0u
#define PARTICLE_LIGHT_MODE_LIGHT 0x2000000u
#define PARTICLE_LIGHT_MODE_LIGHTSHADOW 0x4000000u
#define PARTICLE_LIGHT_CULLED 0x8000000u

// state
#define PARTICLE_STATE_MASK 0xF000u
#define PARTICLE_STATE_ALIVE 0x1000u
#define PARTICLE_STATE_ALLOCATED 0x2000u
#define PARTICLE_STATE_ACCELERATING 0x4000u
#define PARTICLE_STATE_NEED_INIT 0x8000u

#define PARTICLE_MODULATION_MODE_LIFETIME 0x8000000u
#define PARTICLE_MODULATION_MODE_SPEED 0x10000000u

// type
#define PARTICLE_TYPE_COUNT 3
#define PARTICLE_TYPE_MASK 0x300000u
#define PARTICLE_TYPE_1 0x0u
#define PARTICLE_TYPE_2 0x100000u
#define PARTICLE_TYPE_3 0x200000u

// billboard mode
#define PARTICLE_BILLBOARD_MODE_MASK 0x800000u
#define PARTICLE_BILLBOARD_MODE_SCREENALIGN 0x0u
#define PARTICLE_BILLBOARD_MODE_VELOCITYORIENT 0x800000u

// collision
#define PARTICLE_COLLIDE_DEPTHBUFFER 0x400000u
#define PARTICLES_COLLISION_THRESHOLD 0.5f
#define PARTICLE_COLLIDED 0x10000u

// section management
#define PARTICLES_BUFFER_SECTION_COUNT PARTICLE_LIGHT_MODE_COUNT
#define PARTICLES_SWAP_ATTEMPTS_COUNT 2048
#define PARTICLES_SWAP_FAILURE_ATTEMPTS_COUNT PARTICLES_SWAP_ATTEMPTS_COUNT
#define PARTICLES_SWAP_HELP_THRESHOLD 6
#define PARTICLES_SWAP_SELF_RESOLVE_THRESHOLD 8

#define PARTICLES_HW_RASTERIZATION_THRESHOLD 1024u // up to 32x32 quads are software rasterized
#define PARTICLES_MAX_SET_SCALE 64.f
#define PARTICLES_NEIGHBOR_MAX_DISTANCE 0.5f

#define QUAD_VERTEX_COUNT 4

struct ParticleSimulationData
{
    uint DepthBufferIndex;
    uint Seed;
};

#define PARTICLE_DATA_STRIDE 5
struct ParticleDataPacked
{
    uint2 VelocityAge;
    uint2 Position;
    float AnimTime;
};

struct ParticleTransparencyNodePacked
{
    uint Color;
    float Depth;
};

struct ParticleTransparencyNode
{
    float4 Color;
    float Depth;
};

struct ParticleSet
{
    float4 Size; // w - particle scale
    float3 Position;
    float ParticlesSpawnedPerSecond;
    float3 ParticleColor;
    float LightRadius;
    float InitialAge;
    uint MaxSizeAndSpeed;
    uint IsAllocated;
    uint SetFlags; // determines the type, light mode and modulation mode
    uint MaxAllocatedParticles;
    uint TextureIndex;
    uint ParticleStartIndex;
    uint _pad;
};

struct ParticleBufferState
{
    uint ParticleCount[PARTICLES_BUFFER_SECTION_COUNT];
    uint AllocationCounter;
    uint ChangedAttributesMask;
    uint LastAllocatedSetIndex;
    uint PrevLastAllocatedSetIndex;
    
    uint ParticleSectionIndex[PARTICLES_BUFFER_SECTION_COUNT * 2];
    uint MaxParticles[MAX_PARTICLE_SET_COUNT];
    uint PrevParticleFlags[MAX_PARTICLE_SET_COUNT];
};

struct ParticleSystemInfo
{
    uint SectionAliveCount[PARTICLES_BUFFER_SECTION_COUNT];
    uint SectionAllocatedCount[PARTICLES_BUFFER_SECTION_COUNT];
    uint VisibleCount;
    uint VisibleLightCount;
};

#endif