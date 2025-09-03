#include "D3D12PostProcess.h"
#include "D3D12Shaders.h"
#include "D3D12Core.h"
#include "D3D12Surface.h"
#include "D3D12GPass.h"

namespace mofu::graphics::d3d12::fx {
namespace {

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

ID3D12RootSignature* fxRootSig{ nullptr };
ID3D12PipelineState* fxPSO{ nullptr };

u32v2 currentDimensions{ 0, 0 };
D3D12RenderTexture renderTexture{};
constexpr f32 CLEAR_VALUE[4]{ 0.f, 0.f, 0.f, 0.f };

bool
CreatePSO()
{
	assert(!fxRootSig && !fxPSO);

	d3dx::D3D12DescriptorRange range
	{
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND, 0, 0,
		D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE
	};

	d3dx::D3D12RootParameter parameters[FXRootParameterIndices::Count]{};
	parameters[FXRootParameterIndices::GlobalShaderData].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0);
	parameters[FXRootParameterIndices::RootConstants].AsConstants(3, D3D12_SHADER_VISIBILITY_PIXEL, 1);
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

	fxRootSig = rootSigDesc.Create();
	assert(fxRootSig);
	NAME_D3D12_OBJECT(fxRootSig, L"Post-Process FX Root Signature");

	struct {
		d3dx::D3D12PipelineStateSubobjectRootSignature rootSignature{ fxRootSig };
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

	fxPSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
	NAME_D3D12_OBJECT(fxPSO, L"Post-Process FX PSO");

	return fxRootSig && fxPSO;
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
    return CreatePSO();
}

void
Shutdown()
{
	renderTexture.Release();
	core::Release(fxRootSig);
	core::Release(fxPSO);
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
	cmdList->SetGraphicsRoot32BitConstant(idx::RootConstants, gpass::MainBuffer().Srv().index, 0);
	cmdList->SetGraphicsRoot32BitConstant(idx::RootConstants, gpass::DepthBuffer().Srv().index, 1);
	cmdList->SetGraphicsRoot32BitConstant(idx::RootConstants, gpass::NormalBuffer().Srv().index, 2);
	//cmdList->SetGraphicsRootDescriptorTable(idx::DescriptorTable, core::SrvHeap().GpuStart());

	const D3D12_CPU_DESCRIPTOR_HANDLE imageSourceRTV{ renderTexture.Rtv(0) };
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->OMSetRenderTargets(1, &imageSourceRTV, 1, nullptr);
	cmdList->DrawInstanced(3, 1, 0, 0);
	cmdList->OMSetRenderTargets(1, &rtv, 1, nullptr);
	cmdList->DrawInstanced(3, 1, 0, 0);
}

D3D12_GPU_DESCRIPTOR_HANDLE 
GetSrvGPUDescriptorHandle()
{
	return renderTexture.Srv().gpu;
}

}
