#include "D3D12PostProcess.h"
#include "D3D12Shaders.h"
#include "D3D12Core.h"
#include "D3D12Surface.h"
#include "D3D12GPass.h"
#include "Graphics/RenderingDebug.h"
#include "D3D12RayTracing.h"

namespace mofu::graphics::d3d12::fx {
namespace {

constexpr bool CREATE_DEBUG_PSO_AT_LAUNCH{ false };

struct FXRootParameterIndices
{
	enum : u32
	{
		GlobalShaderData = 0,
		RootConstants,
		//DescriptorTable,

		Count
	};
};

struct FXRootParameterIndices_Debug
{
	enum : u32
	{
		GlobalShaderData = 0,
		RootConstants,
		DebugConstants,
		//DescriptorTable,

		Count
	};
};

ID3D12RootSignature* fxRootSig_Default{ nullptr };
ID3D12PipelineState* fxPSO_Default{ nullptr };
ID3D12RootSignature* fxRootSig_Debug{ nullptr };
ID3D12PipelineState* fxPSO_Debug{ nullptr };
ID3D12RootSignature* fxRootSig{ nullptr };
ID3D12PipelineState* fxPSO{ nullptr };

u32v2 currentDimensions{ 0, 0 };
D3D12RenderTexture renderTexture{};
constexpr f32 CLEAR_VALUE[4]{ 0.f, 0.f, 0.f, 0.f };

bool
CreateDebugPSO()
{
	assert(!fxRootSig_Debug && !fxPSO_Debug);

	d3dx::D3D12DescriptorRange range
	{
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND, 0, 0,
		D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE
	};

	d3dx::D3D12RootParameter parameters[FXRootParameterIndices_Debug::Count]{};
	parameters[FXRootParameterIndices_Debug::GlobalShaderData].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0);
	parameters[FXRootParameterIndices_Debug::RootConstants].AsConstants(4, D3D12_SHADER_VISIBILITY_PIXEL, 1);
	parameters[FXRootParameterIndices_Debug::DebugConstants].AsConstants(1, D3D12_SHADER_VISIBILITY_PIXEL, 2);
	//parameters[FXRootParameterIndices::DescriptorTable].AsDescriptorTable(D3D12_SHADER_VISIBILITY_PIXEL, &range, 1);

	struct D3D12_STATIC_SAMPLER_DESC samplers[]
	{
		d3dx::StaticSampler(d3dx::SamplerState.STATIC_POINT, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL),
		d3dx::StaticSampler(d3dx::SamplerState.STATIC_LINEAR, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL),
	};

	d3dx::D3D12RootSignatureDesc rootSigDesc
	{
		&parameters[0], _countof(parameters), d3dx::D3D12RootSignatureDesc::DEFAULT_FLAGS,
		&samplers[0], _countof(samplers)
	};

	rootSigDesc.Flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	fxRootSig_Debug = rootSigDesc.Create();
	assert(fxRootSig_Debug);
	NAME_D3D12_OBJECT(fxRootSig_Debug, L"Post-Process FX Root Signature (DEBUG)");

	struct {
		d3dx::D3D12PipelineStateSubobjectRootSignature rootSignature{ fxRootSig_Debug };
		d3dx::D3D12PipelineStateSubobjectVS vs{ shaders::GetEngineShader(shaders::EngineShader::FullscreenTriangleVS) };
		d3dx::D3D12PipelineStateSubobjectPS ps{ shaders::GetDebugEngineShader(shaders::EngineDebugShader::PostProcessPS) };
		d3dx::D3D12PipelineStateSubobjectPrimitiveTopology primitiveTopology{ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE };
		d3dx::D3D12PipelineStateSubobjectRenderTargetFormats renderTargetFormats{};
		d3dx::D3D12PipelineStateSubobjectRasterizer rasterizer{ d3dx::RasterizerState.NO_CULLING };
	} stream;

	D3D12_RT_FORMAT_ARRAY rtfArray{};
	rtfArray.NumRenderTargets = 1;
	rtfArray.RTFormats[0] = D3D12Surface::DEFAULT_BACK_BUFFER_FORMAT;
	stream.renderTargetFormats = rtfArray;

	fxPSO_Debug = d3dx::CreatePipelineState(&stream, sizeof(stream));
	NAME_D3D12_OBJECT(fxPSO_Debug, L"Post-Process FX PSO (DEBUG)");

	return fxRootSig_Debug && fxPSO_Debug;
}

bool
CreatePSO()
{
	assert(!fxRootSig_Default && !fxPSO_Default);

	d3dx::D3D12DescriptorRange range
	{
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND, 0, 0,
		D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE
	};

	d3dx::D3D12RootParameter parameters[FXRootParameterIndices::Count]{};
	parameters[FXRootParameterIndices::GlobalShaderData].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0);
	parameters[FXRootParameterIndices::RootConstants].AsConstants(4, D3D12_SHADER_VISIBILITY_PIXEL, 1);
	//parameters[FXRootParameterIndices::DescriptorTable].AsDescriptorTable(D3D12_SHADER_VISIBILITY_PIXEL, &range, 1);

	struct D3D12_STATIC_SAMPLER_DESC samplers[]
	{
		d3dx::StaticSampler(d3dx::SamplerState.STATIC_POINT, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL),
		d3dx::StaticSampler(d3dx::SamplerState.STATIC_LINEAR, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL),
	};

	d3dx::D3D12RootSignatureDesc rootSigDesc
	{
		&parameters[0], _countof(parameters), d3dx::D3D12RootSignatureDesc::DEFAULT_FLAGS,
		&samplers[0], _countof(samplers)
	};

	rootSigDesc.Flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	fxRootSig_Default = rootSigDesc.Create();
	assert(fxRootSig_Default);
	NAME_D3D12_OBJECT(fxRootSig_Default, L"Post-Process FX Root Signature");

	struct {
		d3dx::D3D12PipelineStateSubobjectRootSignature rootSignature{ fxRootSig_Default };
		d3dx::D3D12PipelineStateSubobjectVS vs{ shaders::GetEngineShader(shaders::EngineShader::FullscreenTriangleVS) };
		d3dx::D3D12PipelineStateSubobjectPS ps{ shaders::GetEngineShader(shaders::EngineShader::PostProcessPS) };
		d3dx::D3D12PipelineStateSubobjectPrimitiveTopology primitiveTopology{ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE };
		d3dx::D3D12PipelineStateSubobjectRenderTargetFormats renderTargetFormats{};
		d3dx::D3D12PipelineStateSubobjectRasterizer rasterizer{ d3dx::RasterizerState.NO_CULLING };
	} stream;

	D3D12_RT_FORMAT_ARRAY rtfArray{};
	rtfArray.NumRenderTargets = 1;
	rtfArray.RTFormats[0] = D3D12Surface::DEFAULT_BACK_BUFFER_FORMAT;
	stream.renderTargetFormats = rtfArray;

	fxPSO_Default = d3dx::CreatePipelineState(&stream, sizeof(stream));
	NAME_D3D12_OBJECT(fxPSO_Default, L"Post-Process FX PSO");

	fxRootSig = fxRootSig_Default;
	fxPSO = fxPSO_Default;
	return fxRootSig_Default && fxPSO_Default;
}

void
CreateImGuiPresentableSRV(u32v2 dimensions)
{
	renderTexture.Release();

	D3D12_RESOURCE_DESC1 desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = dimensions.x;
	desc.Height = dimensions.y;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 0;
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.SampleDesc = { 1, 0 };
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	desc.SamplerFeedbackMipRegion = { 0, 0, 0 };
	{
		D3D12TextureInitInfo texInfo;
		texInfo.desc = &desc;
		texInfo.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		texInfo.clearValue.Format = desc.Format;
		memcpy(&texInfo.clearValue.Color, &CLEAR_VALUE[0], sizeof(CLEAR_VALUE));
		renderTexture = D3D12RenderTexture{ texInfo };
	}
	NAME_D3D12_OBJECT(renderTexture.Resource(), L"Post Processing Buffer");
}

} // anonymous namespace

void
SetBufferSize(u32v2 size)
{
	u32v2& d{ currentDimensions };
	if (size.x > d.x || size.y > d.y)
	{
		d = { std::max(size.x, d.x), std::max(size.y, d.y) };
		CreateImGuiPresentableSRV(size);
		assert(renderTexture.Resource());
	}
}

bool 
Initialize()
{
	if constexpr (CREATE_DEBUG_PSO_AT_LAUNCH) CreateDebugPSO();
    return CreatePSO();
}

void
Shutdown()
{
	renderTexture.Release();
	core::Release(fxRootSig_Default);
	core::Release(fxPSO_Default);
	core::Release(fxRootSig_Debug);
	core::Release(fxPSO_Debug);
}

void 
SetDebug(bool debugOn)
{
	if (debugOn)
	{
		if (!fxRootSig_Debug || !fxPSO_Debug) CreateDebugPSO();
		fxRootSig = fxRootSig_Debug;
		fxPSO = fxPSO_Debug;
	}
	else
	{
		fxRootSig = fxRootSig_Default;
		fxPSO = fxPSO_Default;
	}
}

void
AddTransitionsPrePostProcess(d3dx::D3D12ResourceBarrierList& barriers)
{
	barriers.AddTransitionBarrier(renderTexture.Resource(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void
AddTransitionsPostPostProcess(d3dx::D3D12ResourceBarrierList& barriers)
{
	barriers.AddTransitionBarrier(renderTexture.Resource(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void DoPostProcessing(DXGraphicsCommandList* cmdList, const D3D12FrameInfo& frameInfo, D3D12_CPU_DESCRIPTOR_HANDLE rtv)
{
	cmdList->SetGraphicsRootSignature(fxRootSig);
	cmdList->SetPipelineState(fxPSO);

	using idx = FXRootParameterIndices;
	cmdList->SetGraphicsRootConstantBufferView(idx::GlobalShaderData, frameInfo.GlobalShaderData);
#if RAYTRACING
	cmdList->SetGraphicsRoot32BitConstant(idx::RootConstants, gpass::MainBuffer().SRV().index, 0);
	cmdList->SetGraphicsRoot32BitConstant(idx::RootConstants, gpass::DepthBuffer().SRV().index, 1);
	cmdList->SetGraphicsRoot32BitConstant(idx::RootConstants, gpass::NormalBuffer().SRV().index, 2);
	cmdList->SetGraphicsRoot32BitConstant(idx::RootConstants, rt::MainBufferSRV().index, 3);
#else
	cmdList->SetGraphicsRoot32BitConstant(idx::RootConstants, gpass::MainBuffer().SRV().index, 0);
	cmdList->SetGraphicsRoot32BitConstant(idx::RootConstants, gpass::DepthBuffer().SRV().index, 1);
	cmdList->SetGraphicsRoot32BitConstant(idx::RootConstants, gpass::NormalBuffer().SRV().index, 2);
	cmdList->SetGraphicsRoot32BitConstant(idx::RootConstants, gpass::MainBuffer().SRV().index, 3);
#endif
	if (fxRootSig == fxRootSig_Debug)
	{
		cmdList->SetGraphicsRoot32BitConstant(FXRootParameterIndices_Debug::DebugConstants, debug::GetDebugMode(), 0);
	}
	//cmdList->SetGraphicsRootDescriptorTable(idx::DescriptorTable, core::SrvHeap().GpuStart());

	const D3D12_CPU_DESCRIPTOR_HANDLE imageSourceRTV{ renderTexture.RTV(0) };
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->OMSetRenderTargets(1, &imageSourceRTV, 1, nullptr);
	cmdList->DrawInstanced(3, 1, 0, 0);
	cmdList->OMSetRenderTargets(1, &rtv, 1, nullptr);
	cmdList->DrawInstanced(3, 1, 0, 0);
}

D3D12_GPU_DESCRIPTOR_HANDLE 
GetSrvGPUDescriptorHandle()
{
	return renderTexture.SRV().gpu;
}

}
