#include "D3D12ResolvePass.h"
#include "D3D12Shaders.h"
#include "D3D12Core.h"
#include "D3D12GPass.h"
#include "Graphics/RenderingDebug.h"
#include "D3D12RayTracing.h"
#include "NGX/D3D12DLSS.h"
#include "FFX/SSSR.h"
#include "Graphics/GraphicsTypes.h"
#include "Effects/D3D12KawaseBlur.h"
#include "D3D12Transpacency.h"

namespace mofu::graphics::d3d12::resolve {
namespace {
struct ResolveRootParameterIndices
{
	enum : u32
	{
		GlobalShaderData = 0,
		RootConstants,
		GISettings,
		TransparencyHeadBuffer,
		TransparencyListBuffer,

		Count
	};
};

struct ResolveRootConstants
{
	enum : u32
	{
		GPassMainBufferIndex,
		GPassDepthBufferIndex,
		RTBufferIndex, // for non-raytracing this could be used for anything
		NormalBufferIndex,
		PositionBufferIndex,
		MotionVectorsBufferIndex,
		MiscBufferIndex,
		ReflectionsBufferIndex,
		MaterialPropertiesBufferIndex,
		ReflectionsStrength,
		VB_HalfRes,
		TransparencyListBufferIndex,

		DoTonemap,
		Count
	};
};

struct SSILVBRootParameterIndices
{
	enum : u32
	{
		GlobalShaderData = 0,
		RootConstants,
		GISettings,

		Count
	};
};

ID3D12RootSignature* _rootSig{ nullptr };
ID3D12PipelineState* _pso{ nullptr };
ID3D12RootSignature* ssilvbRootSig{ nullptr };
ID3D12PipelineState* ssilvbPSO{ nullptr };

D3D12RenderTexture _outputBuffer{};
D3D12RenderTexture _ssilvbTarget{};
u32v2 _currentDimensions{};
DXGI_FORMAT OUTPUT_BUFFER_FORMAT{ DXGI_FORMAT_R16G16B16A16_FLOAT };
DXGI_FORMAT SSILVB_BUFFER_FORMAT{ SSILVB_ONLY_AO ? DXGI_FORMAT_R16_FLOAT : DXGI_FORMAT_R16G16B16A16_FLOAT };
constexpr f32 CLEAR_VALUE[4]{ 0.f, 0.f, 0.f, 0.f };

bool CreateRootSignature()
{
	assert(!_rootSig);

	d3dx::D3D12DescriptorRange range
	{
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND, 0, 0,
		D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE
	};

	d3dx::D3D12RootParameter parameters[ResolveRootParameterIndices::Count]{};
	parameters[ResolveRootParameterIndices::GlobalShaderData].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0);
	parameters[ResolveRootParameterIndices::RootConstants].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 1);
	parameters[ResolveRootParameterIndices::GISettings].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 2);
	parameters[ResolveRootParameterIndices::TransparencyHeadBuffer].AsSRV(D3D12_SHADER_VISIBILITY_PIXEL, 0);
	parameters[ResolveRootParameterIndices::TransparencyListBuffer].AsSRV(D3D12_SHADER_VISIBILITY_PIXEL, 1);

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

	_rootSig = rootSigDesc.Create();
	assert(_rootSig);
	NAME_D3D12_OBJECT(_rootSig, L"Resolve Pass Root Signature");

	{
		assert(!ssilvbRootSig);

		d3dx::D3D12RootParameter vbParams[SSILVBRootParameterIndices::Count]{};
		vbParams[SSILVBRootParameterIndices::GlobalShaderData].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0);
		vbParams[SSILVBRootParameterIndices::RootConstants].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 1);
		vbParams[SSILVBRootParameterIndices::GISettings].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 2);

		d3dx::D3D12RootSignatureDesc rootSigDesc
		{
			&vbParams[0], _countof(vbParams), d3dx::D3D12RootSignatureDesc::DEFAULT_FLAGS,
			&samplers[0], _countof(samplers)
		};

		rootSigDesc.Flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		ssilvbRootSig = rootSigDesc.Create();
		assert(ssilvbRootSig);
		NAME_D3D12_OBJECT(ssilvbRootSig, L"Post-Process SSILVB Root Signature");
	}

	return _rootSig;
}

bool CreatePSO()
{
	assert(!_pso);
	struct {
		d3dx::D3D12PipelineStateSubobjectRootSignature rootSignature{ _rootSig };
		d3dx::D3D12PipelineStateSubobjectVS vs{ shaders::GetEngineShader(EngineShader::FullscreenTriangleVS) };
		d3dx::D3D12PipelineStateSubobjectPS ps{ shaders::GetEngineShader(EngineShader::MainResolvePS) };
		d3dx::D3D12PipelineStateSubobjectPrimitiveTopology primitiveTopology{ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE };
		d3dx::D3D12PipelineStateSubobjectRenderTargetFormats renderTargetFormats{};
		//d3dx::D3D12PipelineStateSubobjectBlend{ d3dx::BlendState.MSAA };
		d3dx::D3D12PipelineStateSubobjectRasterizer rasterizer{ d3dx::RasterizerState.NO_CULLING };
		//d3dx::D3D12PipelineStateSubobjectSampleDesc sd{ { MSAA_SAMPLE_COUNT, MSAA_SAMPLE_QUALITY } };
		//d3dx::D3D12PipelineStateSubobjectSampleMask sm{ { UINT_MAX } };
	} stream;

	D3D12_RT_FORMAT_ARRAY rtfArray{};
	rtfArray.NumRenderTargets = 1;
	rtfArray.RTFormats[0] = OUTPUT_BUFFER_FORMAT;
	stream.renderTargetFormats = rtfArray;

	_pso = d3dx::CreatePipelineState(&stream, sizeof(stream));
	NAME_D3D12_OBJECT(_pso, L"Resolve Pass PSO");

	{
		struct {
			d3dx::D3D12PipelineStateSubobjectRootSignature rootSignature{ ssilvbRootSig };
			d3dx::D3D12PipelineStateSubobjectVS vs{ shaders::GetEngineShader(EngineShader::FullscreenTriangleVS) };
			d3dx::D3D12PipelineStateSubobjectPS ps{ shaders::GetEngineShader(EngineShader::SSILVB_PS) };
			d3dx::D3D12PipelineStateSubobjectPrimitiveTopology primitiveTopology{ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE };
			d3dx::D3D12PipelineStateSubobjectRenderTargetFormats renderTargetFormats{};
			//d3dx::D3D12PipelineStateSubobjectBlend{ d3dx::BlendState.MSAA };
			d3dx::D3D12PipelineStateSubobjectRasterizer rasterizer{ d3dx::RasterizerState.NO_CULLING };
			//d3dx::D3D12PipelineStateSubobjectSampleDesc sd{ { MSAA_SAMPLE_COUNT, MSAA_SAMPLE_QUALITY } };
			//d3dx::D3D12PipelineStateSubobjectSampleMask sm{ { UINT_MAX } };
		} stream;

		D3D12_RT_FORMAT_ARRAY rtfArray{};
		rtfArray.NumRenderTargets = 1;
		rtfArray.RTFormats[0] = SSILVB_BUFFER_FORMAT;
		stream.renderTargetFormats = rtfArray;

		ssilvbPSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
		NAME_D3D12_OBJECT(ssilvbPSO, L"Post-Process FX PSO");
	}

	return _pso;
}

void CreateOutputBuffer(u32v2 dimensions)
{
	_outputBuffer.Release();

	D3D12_RESOURCE_DESC1 desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = dimensions.x;
	desc.Height = dimensions.y;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 0;
	desc.Format = OUTPUT_BUFFER_FORMAT;
	desc.SampleDesc = { MSAA_SAMPLE_COUNT, MSAA_SAMPLE_QUALITY };
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	desc.SamplerFeedbackMipRegion = { 0, 0, 0 };
	{
		D3D12TextureInitInfo texInfo;
		texInfo.desc = &desc;
		texInfo.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		texInfo.clearValue.Format = desc.Format;
		memcpy(&texInfo.clearValue.Color, &CLEAR_VALUE[0], sizeof(CLEAR_VALUE));
		_outputBuffer = D3D12RenderTexture{ texInfo };
	}
	NAME_D3D12_OBJECT(_outputBuffer.Resource(), L"Resolve Pass Output Buffer");

	_ssilvbTarget.Release();

	desc.Width = dimensions.x;
	desc.Height = dimensions.y;
	desc.Format = SSILVB_BUFFER_FORMAT;
	{
		D3D12TextureInitInfo texInfo;
		texInfo.desc = &desc;
		texInfo.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		texInfo.clearValue.Format = desc.Format;
		memcpy(&texInfo.clearValue.Color, &CLEAR_VALUE[0], sizeof(CLEAR_VALUE));
		_ssilvbTarget = D3D12RenderTexture{ texInfo };
	}
	NAME_D3D12_OBJECT(_ssilvbTarget.Resource(), L"Resolve Pass SSILVB Buffer");
}

} // anonymous namespace

bool Initialize()
{
	return CreateRootSignature() && CreatePSO();
}

void Shutdown()
{
	_outputBuffer.Release();
	_ssilvbTarget.Release();
	core::Release(_rootSig);
	core::Release(_pso);
}

void
SetBufferSize(u32v2 size)
{
	u32v2& d{ _currentDimensions };
	_currentDimensions = size;
	CreateOutputBuffer(size);
	assert(_outputBuffer.Resource());
}

void AddTransitionsPreResolve(d3dx::D3D12ResourceBarrierList& barriers)
{
	barriers.AddTransitionBarrier(_outputBuffer.Resource(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	if (graphics::debug::RenderingSettings.VB_HalfRes)
	{
		barriers.AddTransitionBarrier(_ssilvbTarget.Resource(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
	}
}

void AddTransitionsPostResolve(d3dx::D3D12ResourceBarrierList& barriers)
{
	barriers.AddTransitionBarrier(_outputBuffer.Resource(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void ResolvePass(DXGraphicsCommandList* cmdList, const D3D12FrameInfo& frameInfo, D3D12_CPU_DESCRIPTOR_HANDLE rtv, const D3D12Surface& surface)
{
	u32 ssilvbSrvIndex{ gpass::MotionVecBuffer().SRV().index };
	hlsl::ResolveConstants* shaderParams{ core::CBuffer().AllocateSpace<hlsl::ResolveConstants>() };
#if PATHTRACE_MAIN
	shaderParams->RTBufferIndex = rt::MainBufferSRV().index;
	shaderParams->GPassMainBufferIndex = gpass::MainBuffer().SRV().index;
	shaderParams->GPassDepthBufferIndex = gpass::DepthBuffer().SRV().index;
	shaderParams->NormalBufferIndex = gpass::NormalBuffer().SRV().index;
	shaderParams->PositionBufferIndex = gpass::PositionBuffer().SRV().index;
	shaderParams->MotionVectorsBufferIndex = gpass::MotionVecBuffer().SRV().index;
	shaderParams->MiscBufferIndex = gpass::MiscBuffer().SRV().index;
	shaderParams->ReflectionsBufferIndex = ffx::sssr::ReflectionsBuffer().SRV().index;
	shaderParams->MaterialPropertiesBufferIndex = gpass::MaterialPropertiesBuffer().SRV().index;
	shaderParams->ReflectionsStrength = debug::RenderingSettings.ReflectionsStrength;
	shaderParams->DoTonemap = (u32)graphics::debug::RenderingSettings.ApplyTonemap;
	shaderParams->VB_HalfRes = (u32)graphics::debug::RenderingSettings.VB_HalfRes;
	shaderParams->DisplayAO = (u32)graphics::debug::RenderingSettings.DisplayAO;
	shaderParams->RenderGUI = (u32)graphics::debug::RenderingSettings.RenderGUI;
#else
	shaderParams->RTBufferIndex = gpass::MainBuffer().SRV().index; // dummy
	shaderParams->GPassMainBufferIndex = gpass::MainBuffer().SRV().index;
	shaderParams->GPassDepthBufferIndex = gpass::DepthBuffer().SRV().index;
	shaderParams->NormalBufferIndex = gpass::NormalBuffer().SRV().index;
	shaderParams->PositionBufferIndex = gpass::PositionBuffer().SRV().index;
	shaderParams->MotionVectorsBufferIndex = gpass::MotionVecBuffer().SRV().index;
	shaderParams->MiscBufferIndex = gpass::MiscBuffer().SRV().index; // NOTE: not using it now, instead using as space for vbao
	shaderParams->ReflectionsBufferIndex = ffx::sssr::ReflectionsBuffer().SRV().index;
	shaderParams->MaterialPropertiesBufferIndex = gpass::MaterialPropertiesBuffer().SRV().index;
	shaderParams->ReflectionsStrength = debug::RenderingSettings.ReflectionsStrength;
	shaderParams->DoTonemap = (u32)graphics::debug::RenderingSettings.ApplyTonemap;
	shaderParams->SSSR_Enabled = (u32)(graphics::debug::RenderingSettings.ReflectionsEnabled & graphics::debug::RenderingSettings.Reflections_FFXSSSR);
	shaderParams->VBAO_Enabled = (u32)graphics::debug::RenderingSettings.VBAOEnabled;
	shaderParams->VB_HalfRes = (u32)graphics::debug::RenderingSettings.VB_HalfRes;
	shaderParams->TransparencyListBufferIndex = transparency::GetTransparencyListIndex();
	shaderParams->DisplayAO = (u32)graphics::debug::RenderingSettings.DisplayAO;
	shaderParams->RenderGUI = (u32)graphics::debug::RenderingSettings.RenderGUI;
#endif
	graphics::debug::Settings::SSILVB_Settings* giSettings{ core::CBuffer().AllocateSpace<graphics::debug::Settings::SSILVB_Settings>() };
	memcpy(giSettings, &graphics::debug::RenderingSettings.SSILVB, sizeof(graphics::debug::Settings::SSILVB_Settings));
	
	if (debug::RenderingSettings.VB_HalfRes)
	{
		cmdList->ClearRenderTargetView(_ssilvbTarget.RTV(0), CLEAR_VALUE, 0, nullptr);

		cmdList->SetGraphicsRootSignature(ssilvbRootSig);
		cmdList->SetPipelineState(ssilvbPSO);

		using idx = ResolveRootParameterIndices;
		cmdList->SetGraphicsRootConstantBufferView(idx::GlobalShaderData, frameInfo.GlobalShaderData);
		cmdList->SetGraphicsRootConstantBufferView(idx::RootConstants, core::CBuffer().GpuAddress(shaderParams));

		cmdList->SetGraphicsRootConstantBufferView(idx::GISettings, core::CBuffer().GpuAddress(giSettings));

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		const D3D12_CPU_DESCRIPTOR_HANDLE target{ _ssilvbTarget.RTV(0) };
		cmdList->OMSetRenderTargets(1, &target, 1, nullptr);
		cmdList->DrawInstanced(3, 1, 0, 0);

		d3dx::TransitionResource(_ssilvbTarget.Resource(), cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		ssilvbSrvIndex = _ssilvbTarget.SRV().index;
		if (graphics::debug::RenderingSettings.ApplyDualKawaseBlur)
		{
			effects::ApplyKawaseBlur(_ssilvbTarget.Resource(), _ssilvbTarget.SRV().index, cmdList);
			ssilvbSrvIndex = effects::GetBlurUpResultSrvIndex();
		}

		core::SetRenderSizeViewport(cmdList, surface);
	}

	shaderParams->MiscBufferIndex = ssilvbSrvIndex;

	cmdList->SetGraphicsRootSignature(_rootSig);
	cmdList->SetPipelineState(_pso);

	using idx = ResolveRootParameterIndices;
	cmdList->SetGraphicsRootConstantBufferView(idx::GlobalShaderData, frameInfo.GlobalShaderData);
	cmdList->SetGraphicsRootConstantBufferView(idx::RootConstants, core::CBuffer().GpuAddress(shaderParams));
	cmdList->SetGraphicsRootConstantBufferView(idx::GISettings, core::CBuffer().GpuAddress(giSettings));
	cmdList->SetGraphicsRootShaderResourceView(idx::TransparencyHeadBuffer, transparency::GetTransparencyHeadBufferGPUAddr());
	cmdList->SetGraphicsRootShaderResourceView(idx::TransparencyListBuffer, transparency::GetTransparencyListGPUAddr());

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const D3D12_CPU_DESCRIPTOR_HANDLE imageSourceRTV{ _outputBuffer.RTV(0) };
	cmdList->OMSetRenderTargets(1, &imageSourceRTV, 1, nullptr);
	cmdList->DrawInstanced(3, 1, 0, 0);
}

u32 GetResolveOutputSRV()
{
	return _outputBuffer.SRV().index;
}

DXResource* GetResolveOutputBuffer()
{
	return _outputBuffer.Resource();
}

void ResetShaders()
{
	_pso = nullptr;
	CreatePSO();
	assert(_pso);
}

}