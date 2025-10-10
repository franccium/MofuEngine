#include "D3D12RayTracing.h"
#include "D3D12Shaders.h"
#include "D3D12Camera.h"
#include "Graphics/GraphicsTypes.h"
#include "D3D12Content.h"
#include "D3D12Content/D3D12Geometry.h"
#include "D3D12Content/D3D12Material.h"
#include "D3D12Content/D3D12Texture.h"
#include "GPassCache.h"
#include "Graphics/RTSettings.h"
#include "DXRHelpers.h"
#include "D3D12Upload.h"
#include "D3D12GPass.h"
#include "D3D12Content.h"
#include "Graphics/D3D12/D3D12Content/D3D12Geometry.h"
#include "Graphics/D3D12/D3D12Content/D3D12Material.h"
#include "Graphics/D3D12/D3D12Content/D3D12Texture.h"
#include "Editor/RenderingConsole.h"

namespace mofu::graphics::d3d12::rt {
namespace {
struct RTRootParameters
{
	enum : u32
	{
		SceneAC_SRV = 0,
		MainOutput_UAV,
		RayTracingConstants,
		Count
	};
};

constexpr const char* RT_SHADER_FILE{ "RayTracing.hlsl" };

u32 _currentSampleIndex{ 0 };
bool _shouldBuildAccelerationStructure{ true };
bool _wasNewGeometryAdded{ true };
u64 _lastAccelerationStructureBuildFrame{ U64_INVALID_ID };
u64 _accStructVertexCount{ 0 };
u64 _accStructIndexCount{ 0 };

D3D12RenderTexture _rtMainBuffer{};
ID3D12RootSignature* _rtRootSig{ nullptr };
ID3D12StateObject* _rtPSO{ nullptr };

StructuredBuffer _rayGenShaderTable{};
StructuredBuffer _missShaderTable{};
StructuredBuffer _hitShaderTable{};

RawBuffer _topLevelAccStructure;
RawBuffer _bottomLevelAccStructure;
RawBuffer _accScratchBuffer;
util::FreeList<RawBuffer> _scratchBuffers;
util::FreeList<RawBuffer> _blasBuffers;

DXResource* _instanceBufferResource{ nullptr };
StructuredBuffer _geometryInfoBuffer;

constexpr f32 CLEAR_VALUE[4]{ 0.f, 0.f, 0.f, 0.f };

void
BuildAccelerationStructure(DXGraphicsCommandList* const cmdList)
{
	core::Release(_instanceBufferResource);
	content::geometry::CreateGlobalBuffers();
	//TODO:
	const StructuredBuffer& vertexBuffer{ content::geometry::GlobalVertexBuffer() };
	const FormattedBuffer& indexBuffer{ content::geometry::GlobalIndexBuffer() };
	_accStructVertexCount = vertexBuffer.Size();
	_accStructIndexCount = indexBuffer.Size() / sizeof(u16);

	const u32 meshCount{ ecs::scene::GetEntityCount<ecs::component::PathTraceable>() };
	assert(meshCount);
	Array<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs{ meshCount };
	const u32 geometryCount{ (u32)geometryDescs.size() };
	Array<hlsl::GeometryInfo> geometryInfoBufferData{ geometryCount };
	Vec<D3D12_RAYTRACING_INSTANCE_DESC> instances{};

	const gpass::GPassCache& frameCache{ gpass::GetGPassFrameCache() };
	D3D12FrameInfo& frameInfo{ gpass::GetCurrentD3D12FrameInfo() };


	const content::render_item::RenderItemsCache renderItemCache{ frameCache.GetRenderItemsCache() };
	content::render_item::GetRenderItems(frameCache.D3D12RenderItemIDs.data(), meshCount, renderItemCache);
	const content::geometry::SubmeshViewsCache submeshViewCache{ frameCache.GetSubmeshViewsCache() };
	content::geometry::GetSubmeshViews(frameCache.SubmeshGpuIDs, meshCount, submeshViewCache);
	const content::material::MaterialsCache materialsCache{ frameCache.GetMaterialsCache() };
	u32 descriptorIndexCount{ 0 };
	content::material::GetMaterials(frameCache.MaterialIDs, meshCount, materialsCache, descriptorIndexCount);

	//const Vec<content::geometry::MeshInfo>& meshInfos{ content::geometry::GlobalMeshInfos() };
	u32 meshIdx{ 0 };
	for (auto [entity, m, wt, ritem] : ecs::scene::GetRO<ecs::component::PathTraceable, ecs::component::WorldTransform, ecs::component::RenderMesh>())
	{
		const bool isOpaque{ true }; // TODO: opacity

		D3D12_RAYTRACING_GEOMETRY_DESC& geom{ geometryDescs[meshIdx] };
		const u32 vertexOffset{ m.MeshInfo.VertexGlobalOffset };
		const u32 indexOffset{ m.MeshInfo.IndexGlobalOffset };
		geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geom.Flags = isOpaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
		geom.Triangles.IndexBuffer = indexBuffer.GpuAddress() + indexOffset;
		geom.Triangles.IndexCount = m.MeshInfo.IndexCount;
		geom.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT; //TODO: R32
		geom.Triangles.VertexBuffer.StartAddress = vertexBuffer.GpuAddress() + vertexOffset * vertexBuffer.Stride();
		geom.Triangles.VertexBuffer.StrideInBytes = vertexBuffer.Stride();
		geom.Triangles.VertexCount = m.MeshInfo.VertexCount;
		geom.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		geom.Triangles.Transform3x4 = 0;

		hlsl::GeometryInfo& info{ geometryInfoBufferData[meshIdx] };
		info.VertexOffset = vertexOffset;
		info.IndexOffset = indexOffset;
		//TODO: these are not setup yet when building for the first time
		//info.MaterialIndex = *materialsCache.DescriptorIndices[ritem.RenderItemID]; 
		info.MaterialIndex = 0;

		instances.emplace_back();
		D3D12_RAYTRACING_INSTANCE_DESC& instance{ instances.back() };
		instance.InstanceID = entity & 0x00FFFFFF;
		instance.InstanceMask = 1;
		instance.InstanceContributionToHitGroupIndex = 0;
		assert(m.BLASGpuAddress);
		instance.AccelerationStructure = m.BLASGpuAddress;
		instance.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

		xmmat mat{ DirectX::XMLoadFloat4x4(&wt.TRS) };
		DirectX::XMFLOAT3X4 mat3x4{};
		DirectX::XMStoreFloat3x4(&mat3x4, mat);
		memcpy(&instance.Transform, &mat3x4, sizeof(instance.Transform));

		meshIdx++;
	}

	assert(false && "ADD SUPPORT FOR BLAS PER EACH UNIQUE MESH (the blas free list, same idea as textures and meshes), AND SUPPORT FOR INSTANCING TOO SINCE IT REQUIRES THAT ANYWAYS");
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags{ 
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE 
		| D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE };

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topPrebuildInfo{};
	{
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildInfoDesc{};
		prebuildInfoDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		prebuildInfoDesc.Flags = buildFlags;
		prebuildInfoDesc.NumDescs = 1;
		prebuildInfoDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		// InstanceDescs not accessed by prebuild
		core::Device()->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildInfoDesc, &topPrebuildInfo);
		assert(topPrebuildInfo.ResultDataMaxSizeInBytes);
	}

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomPrebuildInfo{};
	{
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildInfoDesc{};
		prebuildInfoDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		prebuildInfoDesc.Flags = buildFlags;
		prebuildInfoDesc.NumDescs = geometryCount;
		prebuildInfoDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		prebuildInfoDesc.pGeometryDescs = geometryDescs.data();
		core::Device()->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildInfoDesc, &bottomPrebuildInfo);
		assert(bottomPrebuildInfo.ResultDataMaxSizeInBytes);
	}

	{
		RawBufferInitInfo info{};
		info.ElementCount = std::max(topPrebuildInfo.ScratchDataSizeInBytes, bottomPrebuildInfo.ScratchDataSizeInBytes) / RawBuffer::Stride;
		info.CreateUAV = true;
		info.InitialState = D3D12_RESOURCE_STATE_COMMON;
		info.Name = L"RT Scratch Buffer";
		_accScratchBuffer.Initialize(info);
	}
	{
		RawBufferInitInfo info{};
		info.ElementCount = topPrebuildInfo.ResultDataMaxSizeInBytes / RawBuffer::Stride;
		info.CreateUAV = true;
		info.InitialState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		info.Name = L"RT Top Level AccStructure Buffer";
		_topLevelAccStructure.Initialize(info);
	}
	{
		RawBufferInitInfo info{};
		info.ElementCount = bottomPrebuildInfo.ResultDataMaxSizeInBytes / RawBuffer::Stride;
		info.CreateUAV = true;
		info.InitialState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		info.Name = L"RT Bottom Level AccStructure Buffer";
		_bottomLevelAccStructure.Initialize(info);
	}

	assert(!_instanceBufferResource);
	_instanceBufferResource = d3dx::CreateResourceBuffer(instances.data(), instances.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
	
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomBuildDesc{};
	{
		bottomBuildDesc.ScratchAccelerationStructureData = _accScratchBuffer.GpuAddress();
		bottomBuildDesc.DestAccelerationStructureData = _bottomLevelAccStructure.GpuAddress();
		bottomBuildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		bottomBuildDesc.Inputs.Flags = buildFlags;
		bottomBuildDesc.Inputs.NumDescs = geometryCount;
		bottomBuildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		bottomBuildDesc.Inputs.pGeometryDescs = geometryDescs.data();
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topBuildDesc{};
	{
		topBuildDesc.ScratchAccelerationStructureData = _accScratchBuffer.GpuAddress();
		topBuildDesc.DestAccelerationStructureData = _topLevelAccStructure.GpuAddress();
		topBuildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		topBuildDesc.Inputs.Flags = buildFlags;
		topBuildDesc.Inputs.NumDescs = instances.size();
		topBuildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		topBuildDesc.Inputs.pGeometryDescs = nullptr;
		topBuildDesc.Inputs.InstanceDescs = _instanceBufferResource->GetGPUVirtualAddress();
	}

	cmdList->BuildRaytracingAccelerationStructure(&bottomBuildDesc, 0, nullptr);
	d3dx::ApplyUAVBarrier(_bottomLevelAccStructure.Buffer(), cmdList); // need to apply before TL build

	cmdList->BuildRaytracingAccelerationStructure(&topBuildDesc, 0, nullptr);
	d3dx::ApplyUAVBarrier(_topLevelAccStructure.Buffer(), cmdList);

	_shouldBuildAccelerationStructure = false;
	_lastAccelerationStructureBuildFrame = core::CurrentCPUFrame();

	StructuredBufferInitInfo geometryInfoBufferInfo{};
	geometryInfoBufferInfo.ElementCount = geometryCount;
	geometryInfoBufferInfo.Stride = sizeof(hlsl::GeometryInfo);
	geometryInfoBufferInfo.Name = L"RT Geometry Info Buffer";
	geometryInfoBufferInfo.InitialData = geometryInfoBufferData.data();
	_geometryInfoBuffer.Initialize(geometryInfoBufferInfo);

	editor::debug::UpdateAccelerationStructureData(_accStructVertexCount, _accStructIndexCount, _lastAccelerationStructureBuildFrame);
}

} // anonymous namespace

void
Update(bool shouldRestartPathTracing, bool renderItemsUpdated)
{
	if (shouldRestartPathTracing)
	{
		_currentSampleIndex = 0;
	}
	_shouldBuildAccelerationStructure |= renderItemsUpdated;
}

DescriptorHandle MainBufferSRV()
{
	return _rtMainBuffer.SRV();
}

void
Render(const D3D12FrameInfo& frameInfo, DXGraphicsCommandList* const cmdList)
{
	if (_currentSampleIndex >= graphics::rt::settings::PP_SAMPLE_COUNT) return;
	if (_shouldBuildAccelerationStructure)
	{
		BuildAccelerationStructure(cmdList);
	}
	
	cmdList->SetComputeRootSignature(_rtRootSig);
	cmdList->SetComputeRootShaderResourceView(RTRootParameters::SceneAC_SRV, _topLevelAccStructure.GpuAddress());
	cmdList->SetComputeRootDescriptorTable(RTRootParameters::MainOutput_UAV, core::CreateTemporaryDescriptorTable(_rtMainBuffer.UAV(), 1));

	hlsl::RayTracingConstants* rtc{ core::CBuffer().AllocateSpace<hlsl::RayTracingConstants>() };
	//FIXME: rowmajor
	DirectX::XMStoreFloat4x4(&rtc->InvViewProjection, frameInfo.Camera->InverseViewProjection());
	DirectX::XMStoreFloat3(&rtc->CameraPos_WS, frameInfo.Camera->Position());
	rtc->SunDirection_WS = graphics::rt::settings::SunDirection;
	memcpy(&rtc->SunIrradiance, &graphics::rt::settings::SunIrradiance, sizeof(v3));
	rtc->SunColor = graphics::rt::settings::SunColor;
	rtc->CosSunAngularRadius = cosf(graphics::rt::settings::SunAngularRadius);
	rtc->SinSunAngularRadius = sinf(graphics::rt::settings::SunAngularRadius);
	rtc->CurrentSampleIndex = _currentSampleIndex;
	rtc->TotalPixelCount = frameInfo.SurfaceWidth * frameInfo.SurfaceHeight;
	rtc->VertexBufferIndex = content::geometry::GlobalVertexBuffer().SRV(0).index;
	rtc->IndexBufferIndex = content::geometry::GlobalIndexBuffer().SRV(0).index;
	rtc->MaterialBufferIndex = 0; // TODO: global surfaces
	rtc->GeometryInfoBufferIndex = _geometryInfoBuffer.SRV(0).index;
	rtc->SkyCubemapIndex = light::GetAmbientLight(0).DiffuseSrvIndex;
	rtc->LightCount = light::GetCullableLightsCount(0);
	//TODO: lights
	cmdList->SetComputeRootConstantBufferView(RTRootParameters::RayTracingConstants, core::CBuffer().GpuAddress(rtc));

	d3dx::D3D12ResourceBarrierList barriers{};
	barriers.AddTransitionBarrier(_rtMainBuffer.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	barriers.ApplyBarriers(cmdList);

	cmdList->SetPipelineState1(_rtPSO);
	D3D12_DISPATCH_RAYS_DESC rayDispatch{};
	rayDispatch.RayGenerationShaderRecord = _rayGenShaderTable.ShaderRecord(0);
	rayDispatch.MissShaderTable = _missShaderTable.ShaderTable();
	rayDispatch.HitGroupTable = _hitShaderTable.ShaderTable();
	rayDispatch.CallableShaderTable = {};
	rayDispatch.Width = frameInfo.SurfaceWidth;
	rayDispatch.Height = frameInfo.SurfaceHeight;
	rayDispatch.Depth = 1;
	cmdList->DispatchRays(&rayDispatch);

	barriers.AddTransitionBarrier(_rtMainBuffer.Resource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	barriers.ApplyBarriers(cmdList);

	++_currentSampleIndex;
	editor::debug::UpdatePathTraceSampleData(_currentSampleIndex);
}

bool
CreateBuffers(u32v2 size)
{
	assert(size.x && size.y);

	_rtMainBuffer.Release();

	D3D12_RESOURCE_DESC1 desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0; // 64KB (or 4MB for multi-sampled textures)
	desc.Width = size.x;
	desc.Height = size.y;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 0;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.SampleDesc = { 1, 0 };
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	desc.SamplerFeedbackMipRegion = { 0, 0, 0 };
	{
		D3D12TextureInitInfo texInfo;
		texInfo.CreateUAV = true;
		texInfo.desc = &desc;
		texInfo.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		texInfo.clearValue.Format = desc.Format;
		memcpy(&texInfo.clearValue.Color, &CLEAR_VALUE[0], sizeof(CLEAR_VALUE));
		_rtMainBuffer = D3D12RenderTexture{ texInfo };
	}
	NAME_D3D12_OBJECT(_rtMainBuffer.Resource(), L"RT Main Buffer");

	return _rtMainBuffer.Resource();
}

bool 
CompileRTShaders()
{
	//NOTE: compiled with others on launch for now
	return true;
}

bool
InitializeRT()
{
	return CreateRootSignature() && CreatePSO();
}

void
Shutdown()
{
	_rtMainBuffer.Release();
	core::Release(_rtRootSig);
	core::Release(_rtPSO);
	_rayGenShaderTable.Release();
	_missShaderTable.Release();
	_hitShaderTable.Release();
	_bottomLevelAccStructure.Release();
	_topLevelAccStructure.Release();
	_geometryInfoBuffer.Release();
}

bool CreateRootSignature()
{
	assert(!_rtRootSig);

	d3dx::D3D12DescriptorRange uavRange
	{
		D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0,
		D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE
	};

	d3dx::D3D12RootParameter parameters[RTRootParameters::Count]{};
	D3D12_ROOT_DESCRIPTOR_FLAGS idk{ D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE }; // TODO: check
	parameters[RTRootParameters::SceneAC_SRV].AsSRV(D3D12_SHADER_VISIBILITY_ALL, 0, 121, idk);
	parameters[RTRootParameters::MainOutput_UAV].AsDescriptorTable(D3D12_SHADER_VISIBILITY_ALL, &uavRange, 1);
	parameters[RTRootParameters::RayTracingConstants].AsCBV(D3D12_SHADER_VISIBILITY_ALL, 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);

	struct D3D12_STATIC_SAMPLER_DESC samplers[]
	{
		d3dx::StaticSampler(d3dx::SamplerState.STATIC_ANISOTROPIC, 0, 0, D3D12_SHADER_VISIBILITY_ALL),
		d3dx::StaticSampler(d3dx::SamplerState.STATIC_LINEAR, 1, 0, D3D12_SHADER_VISIBILITY_ALL),
	};

	d3dx::D3D12RootSignatureDesc rootSigDesc{ &parameters[0], RTRootParameters::Count, D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED, &samplers[0], _countof(samplers) };

	_rtRootSig = rootSigDesc.Create();
	assert(_rtRootSig);
	NAME_D3D12_OBJECT(_rtRootSig, L"RT Root Signature");

	return _rtRootSig;
}

bool CreatePSO()
{
	assert(!_rtPSO);

	constexpr u32 subobjectCount{ 12 };
	RTStateObjectStream stream{ subobjectCount };

	{
		D3D12_DXIL_LIBRARY_DESC dxilDesc{};
		dxilDesc.DXILLibrary = shaders::GetEngineShader(shaders::EngineShader::RayTracingLib);
		stream.AddSubobject(dxilDesc);
	}
	// Hit Groups
	{
		// Primary hit group
		D3D12_HIT_GROUP_DESC hitDesc{};
		hitDesc.HitGroupExport = L"HitGroup";
		hitDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
		hitDesc.ClosestHitShaderImport = L"ClosestHit";
		stream.AddSubobject(hitDesc);
	}
	{
		// Primary alpha-test hit group
		D3D12_HIT_GROUP_DESC hitDesc{};
		hitDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
		hitDesc.ClosestHitShaderImport = L"ClosestHit";
		hitDesc.AnyHitShaderImport = L"AnyHit";
		hitDesc.HitGroupExport = L"AlphaTestHitGroup";
		stream.AddSubobject(hitDesc);
	}
	{
		// Shadow hit group
		D3D12_HIT_GROUP_DESC hitDesc{};
		hitDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
		hitDesc.ClosestHitShaderImport = L"Shadow_ClosestHit";
		hitDesc.HitGroupExport = L"ShadowHitGroup";
		stream.AddSubobject(hitDesc);
	}
	{
		// Shadow alpha-test hit group
		D3D12_HIT_GROUP_DESC hitDesc{};
		hitDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
		hitDesc.ClosestHitShaderImport = L"Shadow_ClosestHit";
		hitDesc.AnyHitShaderImport = L"Shadow_AnyHit";
		hitDesc.HitGroupExport = L"ShadowAlphaTestHitGroup";
		stream.AddSubobject(hitDesc);
	}

	{
		D3D12_RAYTRACING_SHADER_CONFIG shaderConfig{};
		shaderConfig.MaxAttributeSizeInBytes = 2 * sizeof(f32);
		shaderConfig.MaxPayloadSizeInBytes = 4 * sizeof(f32) + 4 * sizeof(u32);   // float3 radiance + float roughness + uint pathLength + uint pixelIdx + uint setIdx + bool IsDiffuse
		stream.AddSubobject(shaderConfig);
	}
	{
		D3D12_GLOBAL_ROOT_SIGNATURE globalRootSigDesc{};
		globalRootSigDesc.pGlobalRootSignature = _rtRootSig;
		stream.AddSubobject(globalRootSigDesc);
	}
	{
		D3D12_RAYTRACING_PIPELINE_CONFIG1 pipelineConfig{};
		pipelineConfig.MaxTraceRecursionDepth = graphics::rt::settings::MAX_TRACE_RECURSION_DEPTH;
		pipelineConfig.Flags = D3D12_RAYTRACING_PIPELINE_FLAG_NONE;
		stream.AddSubobject(pipelineConfig);
	}

	_rtPSO = stream.Create(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

	// get shader identifiers and create shader tables
	ID3D12StateObjectProperties* properties{ nullptr };
	_rtPSO->QueryInterface(IID_PPV_ARGS(&properties));
	const void* rayGenID{ properties->GetShaderIdentifier(L"RayGen") };
	const void* hitGroupID{ properties->GetShaderIdentifier(L"HitGroup") };
	const void* alphaTestHitGroupID{ properties->GetShaderIdentifier(L"AlphaTestHitGroup") };
	const void* shadowHitGroupID{ properties->GetShaderIdentifier(L"ShadowHitGroup") };
	const void* shadowAlphaTestHitGroupID{ properties->GetShaderIdentifier(L"ShadowAlphaTestHitGroup") };
	const void* missID{ properties->GetShaderIdentifier(L"Miss") };
	const void* shadowMissID{ properties->GetShaderIdentifier(L"Shadow_Miss") };

	{
		ShaderIdentifier rayGenRecords[1]{ ShaderIdentifier{rayGenID} };
		StructuredBufferInitInfo info{};
		info.IsShaderTable = true;
		info.Stride = sizeof(ShaderIdentifier);
		info.ElementCount = _countof(rayGenRecords);
		info.InitialData = rayGenRecords;
		info.Name = L"RayGen Shader Table";
		_rayGenShaderTable.Initialize(info);
	}
	{
		ShaderIdentifier missRecords[2]{ ShaderIdentifier{missID}, ShaderIdentifier{shadowMissID} };
		StructuredBufferInitInfo info{};
		info.IsShaderTable = true;
		info.Stride = sizeof(ShaderIdentifier);
		info.ElementCount = _countof(missRecords);
		info.InitialData = missRecords;
		info.Name = L"Miss Shader Table";
		_missShaderTable.Initialize(info);
	}
	{
		const u32 meshCount{ graphics::rt::settings::GetSceneMeshCount() };
		Array<ShaderIdentifier> hitGroupRecords{ meshCount * 2 };
		// TODO: use alpha test hit groyps after i add opacity maps
		for (u32 i{ 0 }; i < meshCount; ++i)
		{
			hitGroupRecords[i] = ShaderIdentifier{ hitGroupID };
			hitGroupRecords[i + 1] = ShaderIdentifier{ shadowHitGroupID };
		}
		StructuredBufferInitInfo info{};
		info.IsShaderTable = true;
		info.Stride = sizeof(ShaderIdentifier);
		info.ElementCount = hitGroupRecords.size();
		info.InitialData = hitGroupRecords.data();
		info.Name = L"Hit Shader Table";
		_hitShaderTable.Initialize(info);
	}

	core::Release(properties);

	return _rtPSO;
}

void RequestRTUpdate()
{
	rt::Update(true, false);
}
void RequestRTAccStructureRebuild()
{
	_shouldBuildAccelerationStructure = true;
}

void 
UpdateAccelerationStructure()
{
	if(!_topLevelAccStructure.Buffer()) return;
	core::Release(_instanceBufferResource);
	const StructuredBuffer& vertexBuffer{ content::geometry::GlobalVertexBuffer() };
	const FormattedBuffer& indexBuffer{ content::geometry::GlobalIndexBuffer() };
	DXGraphicsCommandList* const cmdList{ core::GraphicsCommandList() };
	d3dx::ApplyUAVBarrier(_topLevelAccStructure.Buffer(), cmdList);

	const u32 meshCount{ ecs::scene::GetEntityCount<ecs::component::PathTraceable>() };
	assert(meshCount);
	Array<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs{ meshCount };
	const u32 geometryCount{ (u32)geometryDescs.size() };
	Array<hlsl::GeometryInfo> geometryInfoBufferData{ geometryCount };
	Vec<D3D12_RAYTRACING_INSTANCE_DESC> instances{};

	D3D12FrameInfo& frameInfo{ gpass::GetCurrentD3D12FrameInfo() };

	for (auto [entity, m, wt] : ecs::scene::GetRO<ecs::component::PathTraceable, ecs::component::WorldTransform>())
	{
		instances.emplace_back();
		D3D12_RAYTRACING_INSTANCE_DESC& instance{ instances.back() };
		instance.InstanceID = entity & 0x00FFFFFF;
		instance.InstanceMask = 1;
		instance.AccelerationStructure = _bottomLevelAccStructure.GpuAddress();
		instance.InstanceContributionToHitGroupIndex = 0;
		instance.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

		xmmat mat{ DirectX::XMLoadFloat4x4(&wt.TRS) };
		DirectX::XMFLOAT3X4 mat3x4{};
		DirectX::XMStoreFloat3x4(&mat3x4, mat);
		memcpy(&instance.Transform, &mat3x4, sizeof(instance.Transform));
	}

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags{ 
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE 
		| D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE
		| D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE };

	for (D3D12_RAYTRACING_INSTANCE_DESC& instance : instances)
		instance.AccelerationStructure = _bottomLevelAccStructure.GpuAddress();

	assert(!_instanceBufferResource);
	_instanceBufferResource = d3dx::CreateResourceBuffer(instances.data(), instances.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topBuildDesc{};
	{
		topBuildDesc.ScratchAccelerationStructureData = _accScratchBuffer.GpuAddress();
		topBuildDesc.DestAccelerationStructureData = _topLevelAccStructure.GpuAddress();
		topBuildDesc.SourceAccelerationStructureData = _topLevelAccStructure.GpuAddress();
		topBuildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		topBuildDesc.Inputs.Flags = buildFlags;
		topBuildDesc.Inputs.NumDescs = instances.size();
		topBuildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		topBuildDesc.Inputs.pGeometryDescs = nullptr;
		topBuildDesc.Inputs.InstanceDescs = _instanceBufferResource->GetGPUVirtualAddress();
	}

	core::GraphicsCommandList()->BuildRaytracingAccelerationStructure(&topBuildDesc, 0, nullptr);
	d3dx::ApplyUAVBarrier(_topLevelAccStructure.Buffer(), cmdList);
	_lastAccelerationStructureBuildFrame = core::CurrentCPUFrame();

	editor::debug::UpdateAccelerationStructureData(_accStructVertexCount, _accStructIndexCount, _lastAccelerationStructureBuildFrame);
}

}