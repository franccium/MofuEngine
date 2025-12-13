#include "D3D12ParticleSystem.h"

#include "../D3D12GPass.h"
#include "../D3D12Shaders.h"
#include "../D3D12Transpacency.h"
#include "Utilities/DataPacking.h"
#include "Graphics/GraphicsTypes.h"
#include "Graphics/RenderingDebug.h"

namespace mofu::graphics::d3d12::particles {
namespace {
struct SimulationRootParameters
{
	enum : u32
	{
		GlobalShaderData,
		BufferState,
		SimulationData,
		FlagsBuffer,
		ParticleSetBuffer,
		ParticleDataBuffer,

		TransparencyListHeadBuffer,
		TransparencyListBuffer,
		Count
	};
};

struct ParticleFlags
{
	enum Flags : u32
	{
		None = 0x0u,
		NeedsInit = PARTICLE_STATE_NEED_INIT,
	};
};

constexpr u32 _maxParticleCount{ MAX_PARTICLE_COUNT };
constexpr u32 _maxParticleSetCount{ MAX_PARTICLE_SET_COUNT };

ID3D12RootSignature* _simulationRootSig{ nullptr };
ID3D12PipelineState* _simulationPSO{ nullptr };
u32v2 _threadGroupCount{};

hlsl::particles::ParticleBufferState _particleBufferState;

Vec<hlsl::particles::ParticleSet> _particleSets{};
Vec<ParticleFlags::Flags> _particleFlags{};

StructuredBuffer _particleFlagsBuffer;
StructuredBuffer _particleSetBuffer;
RawBuffer _particleDataBuffer;
RawBuffer _particlesRenderIndirectBuffer;

void 
CreateSimulationPSO()
{
	struct
	{
		d3dx::D3D12PipelineStateSubobjectRootSignature rootSig{ _simulationRootSig };
		d3dx::D3D12PipelineStateSubobjectCS cs{ shaders::GetEngineShader(EngineShader::ParticlesSimulationCS) };
	} stream;

	_simulationPSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
	NAME_D3D12_OBJECT(_simulationPSO, L"Particle Simulation PSO");
}

} // anonymous namespace

void 
TestParticleData()
{
	enum ParticleType : u32
	{
		Type1 = PARTICLE_TYPE_1,
		Type2 = PARTICLE_TYPE_1,
		Type3 = PARTICLE_TYPE_3,
	};

	enum ParticleModulationMode : u32
	{
		None = 0x0u,
		Lifetime = PARTICLE_MODULATION_MODE_LIFETIME,
		Speed = PARTICLE_MODULATION_MODE_SPEED
	};

	enum ParticleLightMode : u32
	{
		NoLight = PARTICLE_LIGHT_MODE_NONE,
		Light = PARTICLE_LIGHT_MODE_LIGHT,
		LightAndShadow = PARTICLE_LIGHT_MODE_LIGHTSHADOW
	};

	//TODO: engine exposed, editor exposed
	struct ParticleSetData
	{
		v4 Size;
		v3 Position;
		f32 ParticlesSpawnedPerSecond;
		v3 ParticleColor;
		f32 LightRadius;
		f32 InitialAge;
		f32 MaxSpeed;
		f32 MaxSize;
		u32 IsAllocated;

		ParticleType Type;
		ParticleLightMode LightMode;
		ParticleModulationMode ModulationMode;

		u32 MaxAllocatedParticles;
		u32 TextureIndex;
		u32 ParticleStartIndex;
	};

	constexpr u32 TEST_SET_COUNT{ 2 };
	Array<ParticleSetData> setsData{ TEST_SET_COUNT };
	ParticleSetData& data1{ setsData[0] };
	data1.Size = v4{ 1.f, 1.f, 1.f, 1.f };
	data1.Position = v3{ 1.f, 1.f, 1.f };
	data1.ParticlesSpawnedPerSecond = 5.f;
	data1.ParticleColor = v3{ 1.f, 0.f, 1.f };
	data1.LightRadius = 1.f;
	data1.InitialAge = 10.f;
	data1.MaxSpeed = 1.f;
	data1.MaxSize = 1.f;
	data1.IsAllocated = true;
	data1.Type = ParticleType::Type1;
	data1.LightMode = ParticleLightMode::NoLight;
	data1.ModulationMode = ParticleModulationMode::None;
	data1.MaxAllocatedParticles = 100;
	data1.TextureIndex = 0;
	data1.ParticleStartIndex = 0;

	ParticleSetData& data2{ setsData[1] };
	data2.Size = v4{ 1.f, 1.f, 1.f, 1.f };
	data2.Position = v3{ 0.f, 1.f, 1.f };
	data2.ParticlesSpawnedPerSecond = 5.f;
	data2.ParticleColor = v3{ 0.f, 1.f, 1.f };
	data2.LightRadius = 1.f;
	data2.InitialAge = 10.f;
	data2.MaxSpeed = 1.f;
	data2.MaxSize = 1.f;
	data2.IsAllocated = true;
	data2.Type = ParticleType::Type1;
	data2.LightMode = ParticleLightMode::NoLight;
	data2.ModulationMode = ParticleModulationMode::None;
	data2.MaxAllocatedParticles = 100;
	data2.TextureIndex = 0;
	data2.ParticleStartIndex = 0;

	_particleSets.resize(TEST_SET_COUNT);

	for (u32 i{ 0 }; i < TEST_SET_COUNT; ++i)
	{
		const ParticleSetData& data{ setsData[i] };
		hlsl::particles::ParticleSet& set{ _particleSets[i] };
		set.Size = data.Size;
		set.Position = data.Position;
		set.ParticlesSpawnedPerSecond = data.ParticlesSpawnedPerSecond;
		set.ParticleColor = data.ParticleColor;
		set.LightRadius = data.LightRadius;
		set.InitialAge = data.InitialAge;
		set.MaxSizeAndSpeed = packing::Pack2xFloat1(data.MaxSize, data.MaxSpeed);
		set.IsAllocated = false;
		set.SetFlags = data.Type | data.LightMode | data.ModulationMode;
		set.MaxAllocatedParticles = data.MaxAllocatedParticles;
		set.TextureIndex = data.TextureIndex;
		set.ParticleStartIndex = data.ParticleStartIndex;
	}
	//_particleSets[0].SetFlags |= (PARTICLE_TYPE_1);
	//_particleSets[1].SetFlags |= (PARTICLE_TYPE_2);

	_particleFlags.resize(_maxParticleCount);
	for (u32 i{ 0 }; i < TEST_SET_COUNT; ++i)
	{
		hlsl::particles::ParticleSet& set{ _particleSets[i] };
		set.IsAllocated = true;

		for (u32 p{ set.ParticleStartIndex }; p < set.ParticleStartIndex + (u32)ceil(set.ParticlesSpawnedPerSecond * set.InitialAge); ++p)
		{
			_particleFlags[p] = ParticleFlags::NeedsInit;
			_particleFlags[p] = ParticleFlags::Flags(_particleFlags[p] | i);
		}
	}
}

bool Initialize()
{
	TestParticleData();
	const u32 particlesPerGroup{ PARTICLES_BATCH_SIZE_X * PARTICLES_BATCH_SIZE_Y };
	const u32 threadGroupsNeeded{ (MAX_PARTICLE_COUNT + particlesPerGroup - 1) / particlesPerGroup };
	const u32 threadGroupsInDim{ static_cast<u32>(ceil(sqrt((f32)threadGroupsNeeded))) };
	_threadGroupCount = { threadGroupsInDim , threadGroupsInDim };

	{
		StructuredBufferInitInfo info{};
		info.ElementCount = _maxParticleCount;
		info.Stride = sizeof(u32);
		info.CreateUAV = true;
		info.IsCPUAccessible = false;
		info.InitialState = D3D12_RESOURCE_STATE_COMMON;
		info.InitialData = _particleFlags.data();
		info.Name = L"Particle Flags Buffer";
		_particleFlagsBuffer = StructuredBuffer{ info };

		info.ElementCount = _maxParticleSetCount;
		info.Stride = sizeof(hlsl::particles::ParticleSet);
		info.Name = L"Particle Set Buffer";
		info.InitialData = _particleSets.data();
		_particleSetBuffer = StructuredBuffer{ info };
	}

	{
		RawBufferInitInfo info{};
		info.ElementCount = _maxParticleCount;
		info.Stride = PARTICLE_DATA_STRIDE;
		info.IsCPUAccessible = false;
		info.CreateUAV = true;
		info.Name = L"Particle Data Buffer";
		info.InitialData = nullptr;
		info.InitialState = D3D12_RESOURCE_STATE_COMMON;
		_particleDataBuffer = RawBuffer{ info };
	}

	assert(!_simulationPSO && !_simulationPSO);

	using param = SimulationRootParameters;
	d3dx::D3D12RootParameter parameters[param::Count]{};
	parameters[param::GlobalShaderData].AsCBV(D3D12_SHADER_VISIBILITY_ALL, 0);
	parameters[param::BufferState].AsCBV(D3D12_SHADER_VISIBILITY_ALL, 1);
	parameters[param::SimulationData].AsCBV(D3D12_SHADER_VISIBILITY_ALL, 2);
	parameters[param::FlagsBuffer].AsUAV(D3D12_SHADER_VISIBILITY_ALL, 0);
	parameters[param::ParticleSetBuffer].AsUAV(D3D12_SHADER_VISIBILITY_ALL, 1);
	parameters[param::ParticleDataBuffer].AsUAV(D3D12_SHADER_VISIBILITY_ALL, 2);
	parameters[param::TransparencyListHeadBuffer].AsUAV(D3D12_SHADER_VISIBILITY_ALL, 3);
	parameters[param::TransparencyListBuffer].AsUAV(D3D12_SHADER_VISIBILITY_ALL, 4);

	struct D3D12_STATIC_SAMPLER_DESC samplers[]
	{
		d3dx::StaticSampler(d3dx::SamplerState.STATIC_POINT, 0, 0, D3D12_SHADER_VISIBILITY_ALL),
	};

	_simulationRootSig = d3dx::D3D12RootSignatureDesc{ 
		&parameters[0], param::Count, 
		d3dx::D3D12RootSignatureDesc::DEFAULT_FLAGS, 
		&samplers[0], _countof(samplers)
	}.Create();
	NAME_D3D12_OBJECT(_simulationRootSig, L"Particle Simulation Root Signature");
	
	CreateSimulationPSO();

	return true;
}

void 
Shutdown()
{
	core::Release(_simulationPSO);
	core::Release(_simulationRootSig);
}

void 
UpdateParticleBufferState()
{
	_particleBufferState.AllocationCounter;
	_particleBufferState.ChangedAttributesMask;
	_particleBufferState.LastAllocatedSetIndex;
	_particleBufferState.PrevLastAllocatedSetIndex;

	for (u32 i{ 0 }; i < PARTICLES_BUFFER_SECTION_COUNT; ++i)
	{
		_particleBufferState.ParticleCount[i];
		_particleBufferState.ParticleSectionIndex[i];
		_particleBufferState.ParticleSectionIndex[i + 1];
		_particleBufferState.MaxParticles[i];
		_particleBufferState.PrevParticleFlags[i];
	}
}

void 
Update(DXGraphicsCommandList* const cmdList, const D3D12FrameInfo& frameInfo)
{
	UpdateParticleBufferState();
	transparency::ClearBuffers(cmdList);
	ConstantBuffer& cbuffer{ core::CBuffer() };
	hlsl::particles::ParticleBufferState* const bufferState{ cbuffer.AllocateSpace<hlsl::particles::ParticleBufferState>() };
	memcpy(bufferState, &_particleBufferState, sizeof(hlsl::particles::ParticleBufferState));

	hlsl::particles::ParticleSimulationData* const simulationData{ cbuffer.AllocateSpace<hlsl::particles::ParticleSimulationData>() };
	simulationData->DepthBufferIndex = gpass::DepthBuffer().SRV().index;
	simulationData->Seed = graphics::debug::RenderingSettings.Particles.Seed;

	cmdList->SetComputeRootSignature(_simulationRootSig);
	cmdList->SetPipelineState(_simulationPSO);
	using param = SimulationRootParameters;
	cmdList->SetComputeRootConstantBufferView(param::GlobalShaderData, frameInfo.GlobalShaderData);
	cmdList->SetComputeRootConstantBufferView(param::BufferState, cbuffer.GpuAddress(bufferState));
	cmdList->SetComputeRootConstantBufferView(param::SimulationData, cbuffer.GpuAddress(simulationData));
	cmdList->SetComputeRootUnorderedAccessView(param::FlagsBuffer, _particleFlagsBuffer.GpuAddress());
	cmdList->SetComputeRootUnorderedAccessView(param::ParticleSetBuffer, _particleSetBuffer.GpuAddress());
	cmdList->SetComputeRootUnorderedAccessView(param::ParticleDataBuffer, _particleDataBuffer.GpuAddress());
	auto add{ transparency::GetTransparencyHeadBufferGPUAddr() };
	auto add2{ transparency::GetTransparencyListGPUAddr() };
	cmdList->SetComputeRootUnorderedAccessView(param::TransparencyListHeadBuffer, transparency::GetTransparencyHeadBufferGPUAddr());
	cmdList->SetComputeRootUnorderedAccessView(param::TransparencyListBuffer, transparency::GetTransparencyListGPUAddr());

	cmdList->Dispatch(_threadGroupCount.x, _threadGroupCount.y, 1);
}

void Render()
{

}

void 
AddBarriersForSimulation(d3dx::D3D12ResourceBarrierList& barriers)
{
	barriers.AddTransitionBarrier(_particleFlagsBuffer.Buffer(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	barriers.AddTransitionBarrier(_particleDataBuffer.Buffer(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void 
AddBarriersForRendering(d3dx::D3D12ResourceBarrierList& barriers)
{
	barriers.AddTransitionBarrier(_particleFlagsBuffer.Buffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	barriers.AddTransitionBarrier(_particleDataBuffer.Buffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void ResetShaders()
{
	_simulationPSO = nullptr;
	CreateSimulationPSO();
	assert(_simulationPSO);
}

}