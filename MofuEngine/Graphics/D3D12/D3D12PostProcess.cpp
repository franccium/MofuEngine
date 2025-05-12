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
	parameters[FXRootParameterIndices::RootConstants].AsConstants(2, D3D12_SHADER_VISIBILITY_PIXEL, 1);
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

} // anonymous namespace

bool 
Initialize()
{
    return CreatePSO();
}

void
Shutdown()
{
	core::Release(fxRootSig);
	core::Release(fxPSO);
}

void DoPostProcessing(DXGraphicsCommandList* cmdList, const D3D12FrameInfo& frameInfo, D3D12_CPU_DESCRIPTOR_HANDLE targetRtv)
{
	const u32 frameIndex{ frameInfo.frameIndex };
	
	cmdList->SetGraphicsRootSignature(fxRootSig);
	cmdList->SetPipelineState(fxPSO);

	cmdList->SetGraphicsRootConstantBufferView(FXRootParameterIndices::GlobalShaderData, frameInfo.globalShaderData);
	cmdList->SetGraphicsRoot32BitConstant(FXRootParameterIndices::RootConstants, gpass::MainBuffer().Srv().index, 0);
	cmdList->SetGraphicsRoot32BitConstant(FXRootParameterIndices::RootConstants, gpass::DepthBuffer().Srv().index, 1);
	//cmdList->SetGraphicsRootDescriptorTable(FXRootParameterIndices::DescriptorTable, core::SrvHeap().GpuStart());

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->OMSetRenderTargets(1, &targetRtv, 1, nullptr);
	cmdList->DrawInstanced(3, 1, 0, 0);
}

}
