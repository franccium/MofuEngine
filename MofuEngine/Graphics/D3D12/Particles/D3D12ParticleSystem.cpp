#include "D3D12ParticleSystem.h"

#include "../Shaders/ParticlesTypes.hlsli"

namespace mofu::graphics::d3d12::particles {
namespace {
constexpr u32 _maxParticleCount{ MAX_PARTICLE_COUNT };
constexpr u32 _maxParticleSetCount{ MAX_PARTICLE_SET_COUNT };

StructuredBuffer _particleFlagsBuffer;
StructuredBuffer _particleSetBuffer;
RawBuffer _particleDataBuffer;
RawBuffer _particlesRenderIndirectBuffer;

} // anonymous namespace

bool Initialize()
{
	{
		StructuredBufferInitInfo info{};
		info.ElementCount = _maxParticleCount;
		info.Stride = sizeof(u32);
		info.IsCPUAccessible = false;
		info.InitialState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		info.InitialData = &particleFlagsInitialData;
		info.Name = L"Particle Flags Buffer";
		_particleFlagsBuffer = StructuredBuffer{ info };

		info.ElementCount = _maxParticleSetCount;
		info.Stride = sizeof(ParticleSet);
		info.Name = L"Particle Set Buffer";
		info.InitialData = &particleSetInitialData;
		_particleSetBuffer = StructuredBuffer{ info };
	}

	{
		RawBufferInitInfo info{};
		info.ElementCount = _maxParticleCount;
		info.Stride = PARTICLE_DATA_STRIDE;
		info.IsCPUAccessible = false;
		info.Name = L"Particle Data Buffer";
		info.InitialData = nullptr;
		info.InitialState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		_particleDataBuffer = RawBuffer{ info };
	}

	return true;
}

void Shutdown()
{

}

void Update()
{

}

void Render()
{

}
}