#include "D3D12GPass.h"
#include "D3D12Shaders.h"
#include "D3D12Camera.h"
#include "Graphics/GraphicsTypes.h"
#include "D3D12Content.h"
#include "D3D12Content/D3D12Geometry.h"
#include "D3D12Content/D3D12Material.h"
#include "D3D12Content/D3D12Texture.h"
#include "ECS/Transform.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "GPassCache.h"
#include "Lights/D3D12Light.h"
#include "Lights/D3D12LightCulling.h"
#include "NGX/D3D12DLSS.h"

#include "tracy/Tracy.hpp"
#include "D3D12RayTracing.h"

namespace mofu::graphics::d3d12::gpass {
namespace {

constexpr u32v2 INITIAL_DIMENSIONS{ 16, 16 };

D3D12RenderTexture gpassMainBuffer{};
D3D12DepthBuffer gpassDepthBuffer{};

D3D12RenderTexture normalBuffer{};
D3D12RenderTexture motionVecBuffer{};

u32v2 dimensions{};

D3D12FrameInfo currentD3D12FrameInfo{};

constexpr f32 CLEAR_VALUE[4]{ 0.f, 0.f, 0.f, 0.f };

GPassCache frameCache;


constexpr D3D12_RESOURCE_STATES BUFFER_SAMPLE_STATE{ 
	DLSS_ENABLED ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
constexpr D3D12_RESOURCE_STATES INITIAL_STATE{ BUFFER_SAMPLE_STATE };
constexpr D3D12_RESOURCE_STATES BUFFER_RENDER_STATE{ D3D12_RESOURCE_STATE_RENDER_TARGET };

//bool
//CreateGPassPSO()
//{
//	assert(!gpassRootSig && !gpassPSO);
//
//	d3dx::D3D12RootParameter parameters[1]{};
//	parameters[OpaqueRootParameters::GlobalShaderData].AsConstants(3, D3D12_SHADER_VISIBILITY_PIXEL, 1);
//	//gpassRootSig = d3dx::D3D12RootSignatureDesc{ &parameters[0], 1 }.Create();
//	d3dx::D3D12RootSignatureDesc rootSigDesc{ &parameters[0], 1 };
//	rootSigDesc.Flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
//	gpassRootSig = rootSigDesc.Create();
//	NAME_D3D12_OBJECT(gpassRootSig, L"GPass Root Signature");
//
//	struct {
//		d3dx::D3D12PipelineStateSubobjectRootSignature rootSignature{ gpassRootSig };
//		d3dx::D3D12PipelineStateSubobjectVS vs{ shaders::GetEngineShader(EngineShader::FullscreenTriangleVS) };
//		d3dx::D3D12PipelineStateSubobjectPS ps{ shaders::GetEngineShader(EngineShader::ColorFillPS) };
//		d3dx::D3D12PipelineStateSubobjectPrimitiveTopology primitiveTopology{ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE };
//		d3dx::D3D12PipelineStateSubobjectRenderTargetFormats renderTargetFormats{};
//		d3dx::D3D12PipelineStateSubobjectDepthStencilFormat depthStencilFormat{ DEPTH_BUFFER_FORMAT };
//		d3dx::D3D12PipelineStateSubobjectRasterizer rasterizer{ d3dx::RasterizerState.NO_CULLING };
//		d3dx::D3D12PipelineStateSubobjectDepthStencil depthStencil{ d3dx::DepthState.DISABLED };
//	} stream;
//
//	D3D12_RT_FORMAT_ARRAY rtfArray{};
//	rtfArray.NumRenderTargets = 1;
//	rtfArray.RTFormats[0] = MAIN_BUFFER_FORMAT;
//	stream.renderTargetFormats = rtfArray;
//	gpassPSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
//	NAME_D3D12_OBJECT(gpassPSO, L"GPass PSO");
//
//	return gpassRootSig && gpassPSO;
//}


// NOTE: system used instead now
void
FillPerObjectData(const D3D12FrameInfo& frameInfo, const content::material::MaterialsCache& materialsCache)
{
	static u32 frame{ 0 };
	static u32 frame2{ 0 };

	const GPassCache& cache{ frameCache };
	const u32 renderItemCount{ (u32)cache.Size() };
	ecs::Entity currentEntityID{ id::INVALID_ID };
	hlsl::PerObjectData* currentDataPointer{ nullptr };
	ConstantBuffer& cbuffer{ core::CBuffer() };

	//frame += 1;
	//frame = (frame % 15);
	

	using namespace DirectX;
	for (u32 i{ 0 }; i < renderItemCount; ++i)
	{
		// we can use the same data for groups of render items from the same game entity to save space in the constant buffer
		if (currentEntityID != cache.EntityIDs[i])
		{
			currentEntityID = cache.EntityIDs[i];
			hlsl::PerObjectData data{};

			//TODO: do something that utilizes ecs better
			ecs::component::WorldTransform& transform{ ecs::scene::GetComponent<ecs::component::WorldTransform>(currentEntityID) };
			xmmat transformWorld{ XMLoadFloat4x4(&transform.TRS) };

			//TODO: fill with actual transform data
			//v3 pos{ -3.f, -10.f, 10.f };
			//v3 scale{ 1.f, 1.f, 1.f };
			////f32 scaling{ (f32)frame };
			////v3 scale{ scaling, scaling, scaling };
			//v4 rot{ 0.f, 0.f, 0.f, 0.f };
			//xmm t{ XMLoadFloat3(&pos) };
			//xmm r{ XMLoadFloat4(&rot) };
			//xmm s{ XMLoadFloat3(&scale) };
			//XMMATRIX transformWorld{ XMMatrixAffineTransformation(s, XMQuaternionIdentity(), r, t) };
			XMStoreFloat4x4(&data.World, transformWorld);
			transformWorld.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);
			xmmat inverseWorld{ XMMatrixInverse(nullptr, transformWorld) };
			XMStoreFloat4x4(&data.InvWorld, inverseWorld);


			xmmat world{ XMLoadFloat4x4(&data.World) };
			xmmat wvp{ XMMatrixMultiply(world, frameInfo.Camera->ViewProjection()) };
			XMStoreFloat4x4(&data.WorldViewProjection, wvp);

			const MaterialSurface* const surface{ materialsCache.MaterialSurfaces[i] };
			memcpy(&data.BaseColor, surface, sizeof(MaterialSurface));

			currentDataPointer = cbuffer.AllocateSpace<hlsl::PerObjectData>();
			memcpy(currentDataPointer, &data, sizeof(hlsl::PerObjectData));
		}

		assert(currentDataPointer);
		cache.PerObjectData[i] = cbuffer.GpuAddress(currentDataPointer);
	}
}

// NOTE: system used instead now
void
PrepareRenderFrame(const D3D12FrameInfo& frameInfo)
{
	assert(frameInfo.Info && frameInfo.Camera);
	assert(frameInfo.Info->RenderItemIDs && frameInfo.Info->RenderItemCount);

	using namespace content;
	GPassCache& cache{ frameCache };
	render_item::GetRenderItemIds(*frameInfo.Info, cache.D3D12RenderItemIDs);
	cache.Resize();
	const u32 renderItemCount{ cache.Size() };

	const render_item::RenderItemsCache renderItemCache{ cache.GetRenderItemsCache() };
	render_item::GetRenderItems(cache.D3D12RenderItemIDs.data(), renderItemCount, renderItemCache);

	const geometry::SubmeshViewsCache submeshViewCache{ cache.GetSubmeshViewsCache() };
	geometry::GetSubmeshViews(cache.SubmeshGpuIDs, renderItemCount, submeshViewCache);

	const material::MaterialsCache materialsCache{ cache.GetMaterialsCache() };
	material::GetMaterials(cache.MaterialIDs, renderItemCount, materialsCache, cache.DescriptorIndexCount);

	FillPerObjectData(frameInfo, materialsCache);

	if (cache.DescriptorIndexCount != 0)
	{
		ConstantBuffer& cbuffer{ core::CBuffer() };
		const u32 size{ cache.DescriptorIndexCount * sizeof(u32) };
		u32* const srvIndices{ (u32* const)cbuffer.AllocateSpace(size) };
		u32 srvIndexOffset{ 0 };

		for (u32 i{ 0 }; i < renderItemCount; ++i)
		{
			const u32 textureCount{ cache.TextureCounts[i] };
			cache.SrvIndices[i] = 0;

			if (textureCount != 0)
			{
				const u32* const indices{ cache.DescriptorIndices[i] };
				memcpy(&srvIndices[srvIndexOffset], indices, textureCount * sizeof(u32));
				cache.SrvIndices[i] = cbuffer.GpuAddress(srvIndices + srvIndexOffset);

				srvIndexOffset += textureCount;
			}
		}
	}
}

void
SetRootParametersDepth(DXGraphicsCommandList* cmdList, u32 cacheItemIndex)
{
	GPassCache& cache{ frameCache };

	const MaterialType::type materialType{ cache.MaterialTypes[cacheItemIndex] };
	switch (materialType)
	{
	case MaterialType::Opaque:
	{
		using params = OpaqueRootParameters;
		cmdList->SetGraphicsRootConstantBufferView(params::PerObjectData, cache.PerObjectData[cacheItemIndex]);
		cmdList->SetGraphicsRootShaderResourceView(params::PositionBuffer, cache.PositionBuffers[cacheItemIndex]);
		//TODO: might want to avoid using element buffer in the vertex shader
		cmdList->SetGraphicsRootShaderResourceView(params::ElementBuffer, cache.ElementBuffers[cacheItemIndex]);
	}
	case MaterialType::AlphaTested:
	{
		using params = AlphaTestedRootParameters;
		cmdList->SetGraphicsRootConstantBufferView(params::PerObjectData, cache.PerObjectData[cacheItemIndex]);
		cmdList->SetGraphicsRootShaderResourceView(params::PositionBuffer, cache.PositionBuffers[cacheItemIndex]);
		//TODO: might want to avoid using element buffer in the vertex shader
		cmdList->SetGraphicsRootShaderResourceView(params::ElementBuffer, cache.ElementBuffers[cacheItemIndex]);
	}
	break;
	}
}

void 
SetRootParametersMain(DXGraphicsCommandList* cmdList, u32 cacheItemIndex)
{
	GPassCache& cache{ frameCache };

	const MaterialType::type materialType{ cache.MaterialTypes[cacheItemIndex] };
	switch (materialType)
	{
	case MaterialType::Opaque:
	{
		using params = OpaqueRootParameters;
		cmdList->SetGraphicsRootConstantBufferView(params::PerObjectData, cache.PerObjectData[cacheItemIndex]);
		cmdList->SetGraphicsRootShaderResourceView(params::PositionBuffer, cache.PositionBuffers[cacheItemIndex]);
		cmdList->SetGraphicsRootShaderResourceView(params::ElementBuffer, cache.ElementBuffers[cacheItemIndex]);
		if (cache.TextureCounts[cacheItemIndex] != 0)
		{
			cmdList->SetGraphicsRootShaderResourceView(params::SrvIndices, cache.SrvIndices[cacheItemIndex]);
		}
	}
	break;
	}
}

bool 
CreateAdditionalDataBuffers(u32v2 size)
{
	// Normal Buffer
	D3D12_RESOURCE_DESC1 desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0; // 64KB (or 4MB for multi-sampled textures)
	desc.Width = size.x;
	desc.Height = size.y;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 0;
	desc.Format = NORMAL_BUFFER_FORMAT;
	desc.SampleDesc = { 1, 0 };
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	desc.SamplerFeedbackMipRegion = { 0, 0, 0 };
	{
		D3D12TextureInitInfo texInfo;
		texInfo.desc = &desc;
		texInfo.initialState = INITIAL_STATE;
		texInfo.MSAAEnabled = false;
		texInfo.clearValue.Format = desc.Format;
		memcpy(&texInfo.clearValue.Color, &CLEAR_VALUE[0], sizeof(CLEAR_VALUE));
		normalBuffer = D3D12RenderTexture{ texInfo };
	}
	NAME_D3D12_OBJECT(normalBuffer.Resource(), L"Normal Screen Buffer");

	// Motion Vectors Buffer
	desc.Format = MOTION_VEC_BUFFER_FORMAT;
	{
		D3D12TextureInitInfo texInfo;
		texInfo.desc = &desc;
		texInfo.initialState = INITIAL_STATE;
		texInfo.MSAAEnabled = false;
		texInfo.clearValue.Format = desc.Format;
		texInfo.clearValue.Color[0] = 0.f;
		texInfo.clearValue.Color[1] = 0.f;
		motionVecBuffer = D3D12RenderTexture{ texInfo };
	}
	NAME_D3D12_OBJECT(motionVecBuffer.Resource(), L"Motion Vectors Screen Buffer");

	return normalBuffer.Resource() && motionVecBuffer.Resource();
}

} // anonymous namespace

bool
Initialize()
{
	bool success{ CreateBuffers(INITIAL_DIMENSIONS) };
#if RAYTRACING
	success &= rt::CreateBuffers(INITIAL_DIMENSIONS);
	success &= rt::InitializeRT();
#endif
	return success;
}

void 
Shutdown()
{
	gpassMainBuffer.Release();
	gpassDepthBuffer.Release();
	normalBuffer.Release();
	motionVecBuffer.Release();
#if RAYTRACING
	rt::Shutdown();
#endif
	dimensions = INITIAL_DIMENSIONS;
}

bool 
CreateBuffers(u32v2 size)
{
	assert(size.x && size.y);

	gpassMainBuffer.Release();
	gpassDepthBuffer.Release();
	normalBuffer.Release();
	motionVecBuffer.Release();
	dimensions = size;

	// Create the main buffer
	D3D12_RESOURCE_DESC1 desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0; // 64KB (or 4MB for multi-sampled textures)
	desc.Width = size.x;
	desc.Height = size.y;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = MAIN_BUFFER_FORMAT;
	desc.SampleDesc = { MSAA_SAMPLE_COUNT, MSAA_SAMPLE_QUALITY };
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	desc.SamplerFeedbackMipRegion = { 0, 0, 0 };
	{
		D3D12TextureInitInfo texInfo;
		texInfo.desc = &desc;
		texInfo.initialState = INITIAL_STATE;
		texInfo.clearValue.Format = desc.Format;
		memcpy(&texInfo.clearValue.Color, &CLEAR_VALUE[0], sizeof(CLEAR_VALUE));
		gpassMainBuffer = D3D12RenderTexture{ texInfo };
	}
	NAME_D3D12_OBJECT(gpassMainBuffer.Resource(), L"GPass Main Buffer");
	
	// Create the depth buffer
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	desc.Format = DEPTH_BUFFER_FORMAT;
	desc.MipLevels = 1;
	{
		D3D12TextureInitInfo texInfo;
		texInfo.desc = &desc;
		texInfo.initialState = D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		texInfo.clearValue.Format = desc.Format;
		texInfo.clearValue.DepthStencil.Depth = 0.f;
		texInfo.clearValue.DepthStencil.Stencil = 0;
		gpassDepthBuffer = D3D12DepthBuffer{ texInfo };
	} 
	NAME_D3D12_OBJECT(gpassDepthBuffer.Resource(), L"GPass Depth Buffer");

	return CreateAdditionalDataBuffers(size) && gpassMainBuffer.Resource() && gpassDepthBuffer.Resource();
}

void 
SetBufferSize(u32v2 size)
{
	u32v2& d{ dimensions };
	if (size.x > d.x || size.y > d.y)
	{
		d = { std::max(size.x, d.x), std::max(size.y, d.y) };
		CreateBuffers(size);
#if RAYTRACING
		rt::CreateBuffers(size);
#endif
		assert(gpassMainBuffer.Resource() && gpassDepthBuffer.Resource() && normalBuffer.Resource() && motionVecBuffer.Resource());
	}
}

void
DepthPrepassWorker(DXGraphicsCommandList* cmdList, const D3D12FrameInfo& frameInfo, u32 workStart, u32 workEnd)
{
	char name[25];
	sprintf_s(name, "worker %u", workStart);
	tracy::SetThreadName(name);
	ZoneScopedNC("Depth Prepass Worker", tracy::Color::DarkOrange4);
	const GPassCache& cache{ frameCache };
	ID3D12RootSignature* currentRootSignature{ nullptr };
	ID3D12PipelineState* currentPipelineState{ nullptr };

	for (u32 i{ workStart }; i < workEnd; ++i)
	{
		if (currentRootSignature != cache.RootSignatures[i])
		{
			currentRootSignature = cache.RootSignatures[i];
			cmdList->SetGraphicsRootSignature(currentRootSignature);
			cmdList->SetGraphicsRootConstantBufferView(OpaqueRootParameters::GlobalShaderData, frameInfo.GlobalShaderData);
		}

		if (currentPipelineState != cache.DepthPipelineStates[i])
		{
			currentPipelineState = cache.DepthPipelineStates[i];
			cmdList->SetPipelineState(currentPipelineState);
		}

		SetRootParametersDepth(cmdList, i);

		const D3D12_INDEX_BUFFER_VIEW ibv{ cache.IndexBufferViews[i] };
		const u32 indexCount{ ibv.SizeInBytes >> (ibv.Format == DXGI_FORMAT_R16_UINT ? 1 : 2) };
		cmdList->IASetIndexBuffer(&ibv);
		cmdList->IASetPrimitiveTopology(cache.PrimitiveTopologies[i]);
		cmdList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
	}
}

void 
DoDepthPrepass(DXGraphicsCommandList* const* cmdLists, const D3D12FrameInfo& frameInfo, [[maybe_unused]] u32 firstWorker)
{
	//TODO: testing removed, bring back when needed
	//PrepareRenderFrame(frameInfo);
	ZoneScopedNC("Depth Prepass Distribution", tracy::Color::DarkOrange1);
	constexpr u32 WORKER_COUNT{ DEPTH_WORKERS };
	const GPassCache& cache{ frameCache };
	const u32 renderItemCount = cache.Size();

#if RAYTRACING
	//TODO: why does it leak otherwise
	DepthPrepassWorker(cmdLists[0], frameInfo, 0, renderItemCount);
#else
	const u32 itemsPerThread = (renderItemCount + WORKER_COUNT - 1) / WORKER_COUNT;
	std::thread threads[WORKER_COUNT];

	for (u32 i{ 0 }; i < WORKER_COUNT; ++i)
	{
		const u32 workStart = i * itemsPerThread;
		const u32 workEnd = std::min(workStart + itemsPerThread, renderItemCount);

		if (workStart < workEnd)
		{
			threads[i] = std::thread(DepthPrepassWorker, cmdLists[i], frameInfo, workStart, workEnd);
		}
	}

	for (u32 i{ 0 }; i < WORKER_COUNT; ++i)
	{
		threads[i].join();
	}
#endif
}

void 
Render(DXGraphicsCommandList* cmdList, const D3D12FrameInfo& frameInfo)
{
#if RENDER_2D_TEST
	cmdList->SetGraphicsRootSignature(gpassRootSig);
	cmdList->SetPipelineState(gpassPSO);

	static u32 frame;
	struct
	{
		f32 width;
		f32 height;
		u32 frame;
	} constants{ (f32)frameInfo.SurfaceWidth, (f32)frameInfo.SurfaceHeight, ++frame };

	cmdList->SetGraphicsRoot32BitConstants(OpaqueRootParameters::GlobalShaderData, 3, &constants, 0);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(3, 1, 0, 0);
#else
	const GPassCache& cache{ frameCache };
	const u32 renderItemCount{ cache.Size() };
	
	ID3D12RootSignature* currentRootSignature{ nullptr };
	ID3D12PipelineState* currentPipelineState{ nullptr };
	const u32 frameIndex{ frameInfo.FrameIndex };
	const u32 lightCullingID{ frameInfo.LightCullingID };

	assert(cache.IsValid());

	for (u32 i{ 0 }; i < renderItemCount; ++i)
	{
		if (currentRootSignature != cache.RootSignatures[i])
		{
			currentRootSignature = cache.RootSignatures[i];
			cmdList->SetGraphicsRootSignature(currentRootSignature);
			using idx = OpaqueRootParameters;
			cmdList->SetGraphicsRootConstantBufferView(idx::GlobalShaderData, frameInfo.GlobalShaderData);
			cmdList->SetGraphicsRootShaderResourceView(idx::DirectionalLights, light::GetNonCullableLightBuffer(frameIndex));
			cmdList->SetGraphicsRootShaderResourceView(idx::CullableLights, light::GetCullableLightBuffer(frameIndex));
			cmdList->SetGraphicsRootShaderResourceView(idx::LightGrid, light::GetLightGridOpaqueBuffer(lightCullingID, frameIndex));
			cmdList->SetGraphicsRootShaderResourceView(idx::LightIndexList, light::GetLightIndexListOpaqueBuffer(lightCullingID, frameIndex));
		}

		if (currentPipelineState != cache.GPassPipelineStates[i])
		{
			currentPipelineState = cache.GPassPipelineStates[i];
			cmdList->SetPipelineState(currentPipelineState);
		}

		SetRootParametersMain(cmdList, i);

		const D3D12_INDEX_BUFFER_VIEW ibv{ cache.IndexBufferViews[i] };
		const u32 indexCount{ ibv.SizeInBytes >> (ibv.Format == DXGI_FORMAT_R16_UINT ? 1 : 2) };
		cmdList->IASetIndexBuffer(&ibv);
		cmdList->IASetPrimitiveTopology(cache.PrimitiveTopologies[i]);
		cmdList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
		//log::Info("Draw: index count %u ", indexCount);
	}
#endif
}
constexpr u32 GIZMOS_TEST_COUNT{ 10 };

void
RenderGizmos(DXGraphicsCommandList* cmdList, const D3D12FrameInfo& frameInfo)
{
	const GPassCache& cache{ frameCache };
	const u32 renderItemCount{ GIZMOS_TEST_COUNT };

}

void
MainGPassWorker(DXGraphicsCommandList* cmdList, const D3D12FrameInfo& frameInfo, u32 workStart, u32 workEnd)
{
	char name[25];
	sprintf_s(name, "main gpass worker %u", workStart);
	tracy::SetThreadName(name);
	ZoneScopedNC("Main GPass Worker", tracy::Color::Green1);

	const GPassCache& cache{ frameCache };

	ID3D12RootSignature* currentRootSignature{ nullptr };
	ID3D12PipelineState* currentPipelineState{ nullptr };
	const u32 frameIndex{ frameInfo.FrameIndex };
	const u32 lightCullingID{ frameInfo.LightCullingID };

	assert(cache.IsValid());

	for (u32 i{ workStart }; i < workEnd; ++i)
	{
		if (currentRootSignature != cache.RootSignatures[i])
		{
			currentRootSignature = cache.RootSignatures[i];
			cmdList->SetGraphicsRootSignature(currentRootSignature);
			using idx = OpaqueRootParameters;
			cmdList->SetGraphicsRootConstantBufferView(idx::GlobalShaderData, frameInfo.GlobalShaderData);
			cmdList->SetGraphicsRootShaderResourceView(idx::DirectionalLights, light::GetNonCullableLightBuffer(frameIndex));
			cmdList->SetGraphicsRootShaderResourceView(idx::CullableLights, light::GetCullableLightBuffer(frameIndex));
			cmdList->SetGraphicsRootShaderResourceView(idx::LightGrid, light::GetLightGridOpaqueBuffer(lightCullingID, frameIndex));
			cmdList->SetGraphicsRootShaderResourceView(idx::LightIndexList, light::GetLightIndexListOpaqueBuffer(lightCullingID, frameIndex));
		}

		if (currentPipelineState != cache.GPassPipelineStates[i])
		{
			currentPipelineState = cache.GPassPipelineStates[i];
			cmdList->SetPipelineState(currentPipelineState);
		}

		SetRootParametersMain(cmdList, i);

		const D3D12_INDEX_BUFFER_VIEW ibv{ cache.IndexBufferViews[i] };
		const u32 indexCount{ ibv.SizeInBytes >> (ibv.Format == DXGI_FORMAT_R16_UINT ? 1 : 2) };
		cmdList->IASetIndexBuffer(&ibv);
		cmdList->IASetPrimitiveTopology(cache.PrimitiveTopologies[i]);
		cmdList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
		//log::Info("Draw: index count %u ", indexCount);
	}
}

void 
RenderMT(DXGraphicsCommandList* const* cmdLists, const D3D12FrameInfo& info)
{
	ZoneScopedNC("Depth Prepass Distribution", tracy::Color::DarkOrange1);
	constexpr u32 WORKER_COUNT{ GPASS_WORKERS };
	const GPassCache& cache{ frameCache };
	const u32 renderItemCount = cache.Size();

#if RAYTRACING
	//TODO: why does it leak otherwise
	MainGPassWorker(cmdLists[0], info, 0, renderItemCount);
#else
	const u32 itemsPerThread = (renderItemCount + WORKER_COUNT - 1) / WORKER_COUNT;
	std::thread threads[WORKER_COUNT];

	for (u32 i{ 0 }; i < WORKER_COUNT; ++i)
	{
		const u32 workStart = i * itemsPerThread;
		const u32 workEnd = std::min(workStart + itemsPerThread, renderItemCount);

		if (workStart < workEnd)
		{
			threads[i] = std::thread(MainGPassWorker, cmdLists[i], info, workStart, workEnd);
		}
	}

	for (u32 i{ 0 }; i < WORKER_COUNT; ++i)
	{
		threads[i].join();
	}
#endif
}

void 
AddTransitionsForDepthPrepass(d3dx::D3D12ResourceBarrierList& barriers)
{
	/*barriers.AddTransitionBarrier(gpassMainBuffer.Resource(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		BUFFER_RENDER_STATE, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY);

	barriers.AddTransitionBarrier(gpassDepthBuffer.Resource(),
		D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_DEPTH_WRITE);*/


	barriers.AddTransitionBarrier(gpassDepthBuffer.Resource(),
		D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void 
AddTransitionsForGPass(d3dx::D3D12ResourceBarrierList& barriers)
{
	//barriers.AddTransitionBarrier(gpassMainBuffer.Resource(),
	//	D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
	//	BUFFER_RENDER_STATE, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY);

	//barriers.AddTransitionBarrier(gpassDepthBuffer.Resource(),
	//	D3D12_RESOURCE_STATE_DEPTH_WRITE,
	//	D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers.AddTransitionBarrier(gpassMainBuffer.Resource(),
		BUFFER_SAMPLE_STATE,
		BUFFER_RENDER_STATE);
	barriers.AddTransitionBarrier(normalBuffer.Resource(),
		BUFFER_SAMPLE_STATE,
		BUFFER_RENDER_STATE);
	barriers.AddTransitionBarrier(motionVecBuffer.Resource(),
		BUFFER_SAMPLE_STATE,
		BUFFER_RENDER_STATE);
	//barriers.AddTransitionBarrier(gpassDepthBuffer.Resource(),
	//	D3D12_RESOURCE_STATE_DEPTH_WRITE,
	//	D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void 
AddTransitionsForPostProcess(d3dx::D3D12ResourceBarrierList& barriers)
{
	if constexpr (DLSS_ENABLED)
	{
		barriers.AddTransitionBarrier(gpassMainBuffer.Resource(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		barriers.AddTransitionBarrier(normalBuffer.Resource(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		barriers.AddTransitionBarrier(motionVecBuffer.Resource(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	else
	{
		barriers.AddTransitionBarrier(gpassMainBuffer.Resource(),
			BUFFER_RENDER_STATE,
			BUFFER_SAMPLE_STATE);
		barriers.AddTransitionBarrier(normalBuffer.Resource(),
			BUFFER_RENDER_STATE,
			BUFFER_SAMPLE_STATE);
		barriers.AddTransitionBarrier(motionVecBuffer.Resource(),
			BUFFER_RENDER_STATE,
			BUFFER_SAMPLE_STATE);
	}
}

void
AddTransitionsForDLSS(d3dx::D3D12ResourceBarrierList& barriers)
{
	barriers.AddTransitionBarrier(gpassMainBuffer.Resource(),
		BUFFER_RENDER_STATE,
		BUFFER_SAMPLE_STATE);
	barriers.AddTransitionBarrier(normalBuffer.Resource(),
		BUFFER_RENDER_STATE,
		BUFFER_SAMPLE_STATE);
	barriers.AddTransitionBarrier(motionVecBuffer.Resource(),
		BUFFER_RENDER_STATE,
		BUFFER_SAMPLE_STATE);
}

void 
AddTransitionsAfterPostProcess(d3dx::D3D12ResourceBarrierList& barriers)
{
	if constexpr (DLSS_ENABLED)
	{
		barriers.AddTransitionBarrier(gpassMainBuffer.Resource(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		barriers.AddTransitionBarrier(normalBuffer.Resource(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		barriers.AddTransitionBarrier(motionVecBuffer.Resource(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}
}

void
ClearDepthStencilView(DXGraphicsCommandList* cmdList)
{
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv{ gpassDepthBuffer.Dsv() };
	cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.f, 0, 0, nullptr);
}

void
SetRenderTargetsForDepthPrepass(DXGraphicsCommandList* cmdList)
{
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv{ gpassDepthBuffer.Dsv() };
	//cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.f, 0, 0, nullptr);
	cmdList->OMSetRenderTargets(0, nullptr, 0, &dsv);
}

void
ClearMainBufferView(DXGraphicsCommandList* cmdList)
{
	cmdList->ClearRenderTargetView(gpassMainBuffer.RTV(0), CLEAR_VALUE, 0, nullptr);
	cmdList->ClearRenderTargetView(normalBuffer.RTV(0), CLEAR_VALUE, 0, nullptr);
	cmdList->ClearRenderTargetView(motionVecBuffer.RTV(0), CLEAR_VALUE, 0, nullptr);
}

void 
SetRenderTargetsForGPass(DXGraphicsCommandList* cmdList)
{
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv{ gpassDepthBuffer.Dsv() };
	const D3D12_CPU_DESCRIPTOR_HANDLE renderTargets[]{ gpassMainBuffer.RTV(0), normalBuffer.RTV(0), motionVecBuffer.RTV(0) };
	cmdList->OMSetRenderTargets(_countof(renderTargets), renderTargets, 0, &dsv);
}

D3D12FrameInfo& GetCurrentD3D12FrameInfo()
{
	return currentD3D12FrameInfo;
}

void StartNewFrame(const D3D12FrameInfo& frameInfo)
{
	currentD3D12FrameInfo = frameInfo;
}

GPassCache& GetGPassFrameCache()
{
	return frameCache;
}

const
D3D12RenderTexture& MainBuffer()
{
	return gpassMainBuffer;
}

const
D3D12DepthBuffer& DepthBuffer()
{
	return gpassDepthBuffer;
}

const
D3D12RenderTexture& NormalBuffer()
{
	return normalBuffer;
}

const
D3D12RenderTexture& MotionVecBuffer()
{
	return motionVecBuffer;
}

}
