#include "D3D12LightCulling.h"
#include "Graphics/GraphicsTypes.h"
#include "Graphics/D3D12/D3D12Shaders.h"
#include "Graphics/D3D12/D3D12Camera.h"
#include "D3D12Light.h"
#include "Graphics/Lights/Light.h"
#include "Graphics/D3D12/D3D12GPass.h"

namespace mofu::graphics::d3d12::light {
namespace {
struct LightCullingRootParameter 
{
	enum Parameter : u32
	{
		GlobalShaderData,
		Constants,
		FrustumsOutOrIndexCounter, // either attached as a UAV for the frustum buffer or a UAV for index counter during light culling
		FrustumsIn,
		CullingInfo,
		BoundingSpheres,
		LightGridOpaque,
		LightIndexListOpaque,

		Count
	};
};

struct CullingParameters
{
	D3D12Buffer GridFrustums;
	D3D12Buffer LightGridAndIndexList; // the light index list buffer is offset within it, pointed to by LightIndexListOpaqueBuffer
	UAVClearableBuffer LightIndexCounter;
	hlsl::LightCullingDispatchParameters CalculateGridFrustumsDispatchParams;
	hlsl::LightCullingDispatchParameters LightCullingDispatchParams;
	u32 FrustumCount{ 0 };
	u32 ViewWidth{ 0 };
	u32 ViewHeight{ 0 };
	f32 CameraFOV{ 0.f };
	D3D12_GPU_VIRTUAL_ADDRESS LightIndexListOpaqueBuffer{ 0 };
	bool HasLights{ true };
};

struct LightCuller
{
	CullingParameters Cullers[FRAME_BUFFER_COUNT]{};
};

// NOTE: maximum for the average number of lights per tile
constexpr u32 MAX_LIGHTS_PER_TILE{ 256 };

ID3D12RootSignature* lightCullingRootSignature{ nullptr };
ID3D12PipelineState* gridFrustumsPSO{ nullptr };
ID3D12PipelineState* lightCullingPSO{ nullptr };
// a list of cullers for each D3D12Surface, each time we add a new D3D12Surface, we need to add a new culler
util::FreeList<LightCuller> lightCullers{};

bool
CreateCullingRootSignature()
{
	assert(!lightCullingRootSignature);
	using param = LightCullingRootParameter;
	d3dx::D3D12RootParameter parameters[param::Count]{};
	parameters[param::GlobalShaderData].AsCBV(D3D12_SHADER_VISIBILITY_ALL, 0);
	parameters[param::Constants].AsCBV(D3D12_SHADER_VISIBILITY_ALL, 1);
	parameters[param::FrustumsOutOrIndexCounter].AsUAV(D3D12_SHADER_VISIBILITY_ALL, 0);
	parameters[param::FrustumsIn].AsSRV(D3D12_SHADER_VISIBILITY_ALL, 0);
	parameters[param::CullingInfo].AsSRV(D3D12_SHADER_VISIBILITY_ALL, 1);
	parameters[param::BoundingSpheres].AsSRV(D3D12_SHADER_VISIBILITY_ALL, 2);
	parameters[param::LightGridOpaque].AsUAV(D3D12_SHADER_VISIBILITY_ALL, 1);
	//TODO: others parameters[param::LightGridOpaque].AsUAV(D3D12_SHADER_VISIBILITY_ALL, 2);
	parameters[param::LightIndexListOpaque].AsUAV(D3D12_SHADER_VISIBILITY_ALL, 3);

	lightCullingRootSignature = d3dx::D3D12RootSignatureDesc{ &parameters[0], param::Count }.Create();
	NAME_D3D12_OBJECT(lightCullingRootSignature, L"Light Culling Root Signature");

	return lightCullingRootSignature != nullptr;
}

bool
CreateCullingPSOs()
{
	{
		assert(!gridFrustumsPSO);

		struct
		{
			d3dx::D3D12PipelineStateSubobjectRootSignature rootSig{ lightCullingRootSignature };
			d3dx::D3D12PipelineStateSubobjectCS cs{ shaders::GetEngineShader(shaders::EngineShader::CalculateGridFrustumsCS) };
		} stream;

		gridFrustumsPSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
		NAME_D3D12_OBJECT(gridFrustumsPSO, L"Grid Frustum PSO");
	}
	{
		assert(!lightCullingPSO);

		struct
		{
			d3dx::D3D12PipelineStateSubobjectRootSignature rootSig{ lightCullingRootSignature };
			d3dx::D3D12PipelineStateSubobjectCS cs{ shaders::GetEngineShader(shaders::EngineShader::LightCullingCS) };
		} stream;

		lightCullingPSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
		NAME_D3D12_OBJECT(lightCullingPSO, L"Grid Frustum PSO");
	}
	return gridFrustumsPSO != nullptr && lightCullingPSO != nullptr;
}

void
ResizeBuffers(CullingParameters& culler)
{
	const u32 frustumCount{ culler.FrustumCount };
	const u32 frustumBufferSize{ sizeof(hlsl::ConeFrustum) * frustumCount };

	D3D12BufferInitInfo info{};
	info.alignment = sizeof(v4);
	info.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	if (frustumBufferSize > culler.GridFrustums.Size())
	{
		info.size = frustumBufferSize;
		culler.GridFrustums = D3D12Buffer{ info, false };
		NAME_D3D12_OBJECT_INDEXED(culler.GridFrustums.Buffer(), frustumCount, L"Light Grid Frustums Buffer - count:");
	}

	const u32 lightGridBufferSize{ (u32)math::AlignUp<sizeof(v4)>(sizeof(u32v2) * frustumCount) };
	const u32 lightIndexListBufferSize{ (u32)math::AlignUp<sizeof(v4)>(sizeof(u32) * frustumCount * MAX_LIGHTS_PER_TILE) };
	const u32 lightGridAndIndexListBufferSize{ lightGridBufferSize + lightIndexListBufferSize };
	if (lightGridAndIndexListBufferSize > culler.LightGridAndIndexList.Size())
	{
		info.size = lightGridAndIndexListBufferSize;
		culler.LightGridAndIndexList = D3D12Buffer{ info, false };

		const D3D12_GPU_VIRTUAL_ADDRESS lightGridOpaqueBuffer{ culler.LightGridAndIndexList.GpuAddress() };
		culler.LightIndexListOpaqueBuffer = lightGridOpaqueBuffer + lightGridBufferSize;
		NAME_D3D12_OBJECT_INDEXED(culler.LightGridAndIndexList.Buffer(), lightGridAndIndexListBufferSize, L"Light Culling Grid and Index List Buffer - size:");

		if (!culler.LightIndexCounter.Buffer())
		{
			info = UAVClearableBuffer::GetDefaultInitInfo(1);
			culler.LightIndexCounter = UAVClearableBuffer{ info };
			NAME_D3D12_OBJECT_INDEXED(culler.LightIndexCounter.Buffer(), core::CurrentFrameIndex(), L"Light index Counter Buffer - count:");
		}
	}
}

void
ResizeSurfaceCuller(CullingParameters& culler)
{
	assert(culler.ViewWidth >= LIGHT_CULLING_TILE_SIZE && culler.ViewHeight >= LIGHT_CULLING_TILE_SIZE);
	const u32v2 tileCount{ (u32)math::AlignUp<LIGHT_CULLING_TILE_SIZE>(culler.ViewWidth) / LIGHT_CULLING_TILE_SIZE,
		(u32)math::AlignUp<LIGHT_CULLING_TILE_SIZE>(culler.ViewHeight) / LIGHT_CULLING_TILE_SIZE };
	culler.FrustumCount = tileCount.x * tileCount.y;

	// CalculateGridFrustums dispatch parameters
	{
		hlsl::LightCullingDispatchParameters& params{ culler.CalculateGridFrustumsDispatchParams };
		params.NumThreads = tileCount;
		params.NumThreadGroups.x = (u32)math::AlignUp<LIGHT_CULLING_TILE_SIZE>(tileCount.x) / LIGHT_CULLING_TILE_SIZE;
		params.NumThreadGroups.y = (u32)math::AlignUp<LIGHT_CULLING_TILE_SIZE>(tileCount.y) / LIGHT_CULLING_TILE_SIZE;
	}

	// LightCulling dispatch parameters
	{
		// each group gets one tile and the number of threads is screen width and 
		// height in x and y directions, aligned to LIGHT_CULLING_TILE_SIZE
		hlsl::LightCullingDispatchParameters& params{ culler.LightCullingDispatchParams };
		params.NumThreads.x = tileCount.x * LIGHT_CULLING_TILE_SIZE;
		params.NumThreads.y = tileCount.y * LIGHT_CULLING_TILE_SIZE;
		params.NumThreadGroups = tileCount;
	}

	ResizeBuffers(culler);
}

void
CalculateGridFrustumsDispatch(CullingParameters& culler, DXGraphicsCommandList* const cmdList,
	const D3D12FrameInfo& frameInfo, d3dx::D3D12ResourceBarrierList& barriers)
{
	ConstantBuffer& cbuffer{ core::CBuffer() };
	hlsl::LightCullingDispatchParameters* const buffer{ cbuffer.AllocateSpace<hlsl::LightCullingDispatchParameters>() };
	const hlsl::LightCullingDispatchParameters& params{ culler.CalculateGridFrustumsDispatchParams };
	memcpy(buffer, &params, sizeof(hlsl::LightCullingDispatchParameters));

	constexpr bool VISUALIZE_IN_SHADER{ true };
	barriers.AddTransitionBarrier(culler.GridFrustums.Buffer(),
		VISUALIZE_IN_SHADER ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE 
		: D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	barriers.ApplyBarriers(cmdList);

	using param = LightCullingRootParameter;
	cmdList->SetComputeRootSignature(lightCullingRootSignature);
	cmdList->SetPipelineState(gridFrustumsPSO);
	cmdList->SetComputeRootConstantBufferView(param::GlobalShaderData, frameInfo.GlobalShaderData);
	cmdList->SetComputeRootConstantBufferView(param::Constants, cbuffer.GpuAddress(buffer));
	cmdList->SetComputeRootUnorderedAccessView(param::FrustumsOutOrIndexCounter, culler.GridFrustums.GpuAddress());
	cmdList->Dispatch(params.NumThreadGroups.x, params.NumThreadGroups.y, 1);

	barriers.AddTransitionBarrier(culler.GridFrustums.Buffer(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		VISUALIZE_IN_SHADER ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE 
		: D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void _declspec(noinline)
ResizeAndCalculateGridFrustums(CullingParameters& culler, DXGraphicsCommandList* const cmdList, 
	const D3D12FrameInfo& frameInfo, d3dx::D3D12ResourceBarrierList& barriers)
{
	culler.ViewWidth = frameInfo.SurfaceWidth;
	culler.ViewHeight = frameInfo.SurfaceHeight;
	culler.CameraFOV = frameInfo.Camera->FieldOfView();

	ResizeSurfaceCuller(culler);
	CalculateGridFrustumsDispatch(culler, cmdList, frameInfo, barriers);
}

} // anonymous namespace

bool
InitializeLightCulling()
{
	return CreateCullingRootSignature() && CreateCullingPSOs();
}

void
ShutdownLightCulling()
{
	assert(lightCullingRootSignature && gridFrustumsPSO && lightCullingPSO);
	core::DeferredRelease(lightCullingRootSignature);
	core::DeferredRelease(gridFrustumsPSO);
	core::DeferredRelease(lightCullingPSO);
}

void
CullLights(DXGraphicsCommandList* const cmdList, const D3D12FrameInfo& frameInfo, d3dx::D3D12ResourceBarrierList& barriers)
{
	assert(id::IsValid(frameInfo.LightCullingID));
	CullingParameters& culler{ lightCullers[frameInfo.LightCullingID].Cullers[frameInfo.FrameIndex] };

	if (culler.ViewWidth != frameInfo.SurfaceWidth || culler.ViewHeight != frameInfo.SurfaceHeight
		|| !math::IsEqual(culler.CameraFOV, frameInfo.Camera->FieldOfView()))
	{
		ResizeAndCalculateGridFrustums(culler, cmdList, frameInfo, barriers);
	}

	hlsl::LightCullingDispatchParameters& params{ culler.LightCullingDispatchParams };
	params.NumLights = graphics::light::GetCullableLightsCount(frameInfo.Info->LightSetIdx);
	params.DepthBufferSrvIndex = gpass::DepthBuffer().Srv().index;

	if (params.NumLights == 0 && !culler.HasLights) return;

	culler.HasLights = params.NumLights != 0;

	ConstantBuffer& cbuffer{ core::CBuffer() };
	hlsl::LightCullingDispatchParameters* const buffer{ cbuffer.AllocateSpace<hlsl::LightCullingDispatchParameters>() };
	memcpy(buffer, &params, sizeof(hlsl::LightCullingDispatchParameters));

	// make light grid and light index buffers writable
	barriers.AddTransitionBarrier(culler.LightGridAndIndexList.Buffer(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	barriers.ApplyBarriers(cmdList);

	const u32v4 clearValue{ 0,0,0,0 };
	culler.LightIndexCounter.ClearUAV(cmdList, &clearValue.x);

	cmdList->SetComputeRootSignature(lightCullingRootSignature);
	cmdList->SetPipelineState(lightCullingPSO);
	using param = LightCullingRootParameter;
	cmdList->SetComputeRootConstantBufferView(param::GlobalShaderData, frameInfo.GlobalShaderData);
	cmdList->SetComputeRootConstantBufferView(param::Constants, cbuffer.GpuAddress(buffer));
	cmdList->SetComputeRootUnorderedAccessView(param::FrustumsOutOrIndexCounter, culler.LightIndexCounter.GpuAdress());
	cmdList->SetComputeRootShaderResourceView(param::FrustumsIn, culler.GridFrustums.GpuAddress());
	cmdList->SetComputeRootShaderResourceView(param::CullingInfo, light::GetCullingInfoBuffer(frameInfo.FrameIndex));
	cmdList->SetComputeRootShaderResourceView(param::BoundingSpheres, light::GetBoundingSpheresBuffer(frameInfo.FrameIndex));
	cmdList->SetComputeRootUnorderedAccessView(param::LightGridOpaque, culler.LightGridAndIndexList.GpuAddress());
	cmdList->SetComputeRootUnorderedAccessView(param::LightIndexListOpaque, culler.LightIndexListOpaqueBuffer);

	cmdList->Dispatch(params.NumThreadGroups.x, params.NumThreadGroups.y, 1);

	// NOTE: this transition should be applied by the caller of this function
	barriers.AddTransitionBarrier(culler.LightGridAndIndexList.Buffer(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

id_t 
AddLightCuller()
{
	return lightCullers.add();
}

void 
RemoveLightCuller(id_t cullerID)
{
	assert(id::IsValid(cullerID));
	lightCullers.remove(cullerID);
}

D3D12_GPU_VIRTUAL_ADDRESS
GetGridFrustumsBuffer(u32 lightCullingID, u32 frameIndex)
{
	return lightCullers[lightCullingID].Cullers[frameIndex].GridFrustums.GpuAddress();
}

D3D12_GPU_VIRTUAL_ADDRESS
GetLightGridOpaqueBuffer(u32 lightCullingID, u32 frameIndex)
{
	return lightCullers[lightCullingID].Cullers[frameIndex].LightGridAndIndexList.GpuAddress();
}

D3D12_GPU_VIRTUAL_ADDRESS 
GetLightIndexListOpaqueBuffer(u32 lightCullingID, u32 frameIndex)
{
	return lightCullers[lightCullingID].Cullers[frameIndex].LightIndexListOpaqueBuffer;
}

}
