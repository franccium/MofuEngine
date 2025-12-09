#include "D3D12PostProcess.h"
#include "D3D12Shaders.h"
#include "D3D12Core.h"
#include "D3D12Surface.h"
#include "D3D12GPass.h"
#include "Graphics/RenderingDebug.h"
#include "D3D12RayTracing.h"
#include "NGX/D3D12DLSS.h"
#include "FFX/SSSR.h"
#include "Graphics/GraphicsTypes.h"
#include "Effects/D3D12KawaseBlur.h"
#include "D3D12ResolvePass.h"
#include "EngineAPI/ECS/SceneAPI.h"

namespace mofu::graphics::d3d12::fx {
namespace {

constexpr bool CREATE_DEBUG_PSO_AT_LAUNCH{ false };

struct FXRootParameterIndices
{
	enum : u32
	{
		GlobalShaderData = 0,
		RootConstants,
		GISettings,
		GTTonemapCurve,

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

struct FXRootParameterIndices_Debug
{
	enum : u32
	{
		GlobalShaderData = 0,
		RootConstants,
		GISettings,
		GTTonemapCurve,
		DebugConstants,
		//DescriptorTable,

		Count
	};
};

struct PostProcessRootConstants
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

		DoTonemap,
		Count
	};
};

ID3D12RootSignature* fxRootSig_Default{ nullptr };
ID3D12PipelineState* fxPSO_Default{ nullptr };
ID3D12RootSignature* fxRootSig_Debug{ nullptr };
ID3D12PipelineState* fxPSO_Debug{ nullptr };
ID3D12RootSignature* fxRootSig{ nullptr };
ID3D12PipelineState* fxPSO{ nullptr };

ID3D12RootSignature* ssilvbRootSig{ nullptr };
ID3D12PipelineState* ssilvbPSO{ nullptr };

u32v2 currentDimensions{ 0, 0 };
D3D12RenderTexture renderTexture{};
D3D12RenderTexture ssilvbTarget{};
DXGI_FORMAT SSILVB_BUFFER_FORMAT{ DXGI_FORMAT_R16G16B16A16_FLOAT };
constexpr f32 CLEAR_VALUE[4]{ 0.f, 0.f, 0.f, 0.f };

u32v2 _fxResolution{ 0, 0 };

struct GT7ToneMapCurve
{
	float PeakIntensity;
	float Alpha;
	float MidPoint;
	float LinearSection;
	float ToeStrength;
	float kA;
	float kB;
	float kC;

	void Initialize(float monitorIntensity, float alpha, float grayPoint, float linearSection, float toeStrength)
	{
		PeakIntensity = monitorIntensity;
		Alpha = alpha;
		MidPoint = grayPoint;
		LinearSection = linearSection;
		ToeStrength = toeStrength;

		float k = (linearSection - 1.f) / (alpha - 1.f);
		kA = PeakIntensity * linearSection + PeakIntensity * k;
		kB = -PeakIntensity * k * std::expf(linearSection / k);
		kC = -1.0f / (k * PeakIntensity);
	}
};
GT7ToneMapCurve _tonemapCurve{};
#define HDR 0
constexpr f32 GRAN_TURISMO_SDR_PAPER_WHITE{ 250.0f };
constexpr f32 REFERENCE_LUMINANCE{ 300.0f };
constexpr f32 PhysicalToFrameBufferValue(f32 v) { return v / REFERENCE_LUMINANCE; }

void
CreateDebugRootSignature()
{
	assert(!fxRootSig_Debug);

	d3dx::D3D12DescriptorRange range
	{
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND, 0, 0,
		D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE
	};

	d3dx::D3D12RootParameter parameters[FXRootParameterIndices_Debug::Count]{};
	parameters[FXRootParameterIndices_Debug::GlobalShaderData].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0);
	parameters[FXRootParameterIndices_Debug::RootConstants].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 1);
	parameters[FXRootParameterIndices_Debug::GISettings].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 2);
	parameters[FXRootParameterIndices::GTTonemapCurve].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0, 3);
	parameters[FXRootParameterIndices_Debug::DebugConstants].AsConstants(1, D3D12_SHADER_VISIBILITY_PIXEL, 2);

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
}

bool
CreateDebugPSO()
{
	assert(!fxPSO_Debug && fxRootSig_Debug);

	struct {
		d3dx::D3D12PipelineStateSubobjectRootSignature rootSignature{ fxRootSig_Debug };
		d3dx::D3D12PipelineStateSubobjectVS vs{ shaders::GetEngineShader(EngineShader::FullscreenTriangleVS) };
		d3dx::D3D12PipelineStateSubobjectPS ps{ shaders::GetDebugEngineShader(EngineDebugShader::PostProcessPS) };
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

void
CreateRootSignature()
{
	assert(!fxRootSig_Default);

	d3dx::D3D12DescriptorRange range
	{
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND, 0, 0,
		D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE
	};

	d3dx::D3D12RootParameter parameters[FXRootParameterIndices::Count]{};
	parameters[FXRootParameterIndices::GlobalShaderData].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0);
	parameters[FXRootParameterIndices::RootConstants].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 1);
	parameters[FXRootParameterIndices::GISettings].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 2);
	parameters[FXRootParameterIndices::GTTonemapCurve].AsCBV(D3D12_SHADER_VISIBILITY_PIXEL, 0, 3);

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
}

bool
CreatePSO()
{
	{
		assert(!fxPSO_Default && fxRootSig_Default);
		struct {
			d3dx::D3D12PipelineStateSubobjectRootSignature rootSignature{ fxRootSig_Default };
			d3dx::D3D12PipelineStateSubobjectVS vs{ shaders::GetEngineShader(EngineShader::FullscreenTriangleVS) };
			d3dx::D3D12PipelineStateSubobjectPS ps{ shaders::GetEngineShader(EngineShader::PostProcessPS) };
			d3dx::D3D12PipelineStateSubobjectPrimitiveTopology primitiveTopology{ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE };
			d3dx::D3D12PipelineStateSubobjectRenderTargetFormats renderTargetFormats{};
			//d3dx::D3D12PipelineStateSubobjectBlend{ d3dx::BlendState.MSAA };
			d3dx::D3D12PipelineStateSubobjectRasterizer rasterizer{ d3dx::RasterizerState.NO_CULLING };
			//d3dx::D3D12PipelineStateSubobjectSampleDesc sd{ { MSAA_SAMPLE_COUNT, MSAA_SAMPLE_QUALITY } };
			//d3dx::D3D12PipelineStateSubobjectSampleMask sm{ { UINT_MAX } };
		} stream;

		D3D12_RT_FORMAT_ARRAY rtfArray{};
		rtfArray.NumRenderTargets = 1;
		rtfArray.RTFormats[0] = D3D12Surface::DEFAULT_BACK_BUFFER_FORMAT;
		stream.renderTargetFormats = rtfArray;

		fxPSO_Default = d3dx::CreatePipelineState(&stream, sizeof(stream));
		NAME_D3D12_OBJECT(fxPSO_Default, L"Post-Process FX PSO");

		fxRootSig = fxRootSig_Default;
		fxPSO = fxPSO_Default;
	}

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

	effects::CreatePSOs();

	return fxRootSig_Default && fxPSO_Default;
}

void
CreateFXBuffers()
{
	ssilvbTarget.Release();

	D3D12_RESOURCE_DESC1 desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = _fxResolution.x;
	desc.Height = _fxResolution.y;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 0;
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.SampleDesc = { MSAA_SAMPLE_COUNT, MSAA_SAMPLE_QUALITY };
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	desc.SamplerFeedbackMipRegion = { 0, 0, 0 };
	{
		D3D12TextureInitInfo texInfo;
		texInfo.desc = &desc;
		texInfo.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		texInfo.clearValue.Format = desc.Format;
		memcpy(&texInfo.clearValue.Color, &CLEAR_VALUE[0], sizeof(CLEAR_VALUE));
		ssilvbTarget = D3D12RenderTexture{ texInfo };
	}
	NAME_D3D12_OBJECT(ssilvbTarget.Resource(), L"SSILVB Buffer");
}

#if RENDER_GUI
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
	desc.SampleDesc = { MSAA_SAMPLE_COUNT, MSAA_SAMPLE_QUALITY };
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
#endif

void 
InitializeEffects()
{
#if HDR
	info::DisplayInfo displayInfo{ core::GetDisplayInfo() };
	_tonemapCurve.Initialize(PhysicalToFrameBufferValue(displayInfo.MaxLuminance), 0.25f, 0.538f, 0.444f, 1.280f);
#else
	_tonemapCurve.Initialize(PhysicalToFrameBufferValue(GRAN_TURISMO_SDR_PAPER_WHITE), 0.25f, 0.538f, 0.444f, 1.280f);
#endif

	effects::Initialize();
}

} // anonymous namespace

void
SetBufferSize(u32v2 size)
{
	u32v2& d{ currentDimensions };
	_fxResolution = { size.x / 2, size.y / 2 };
	CreateFXBuffers();
#if RENDER_GUI
	CreateImGuiPresentableSRV(size);
	assert(renderTexture.Resource());
	assert(ssilvbTarget.Resource());
#endif
}

bool 
Initialize()
{
	InitializeEffects();
	if constexpr (CREATE_DEBUG_PSO_AT_LAUNCH)
	{
		CreateDebugRootSignature();
		CreateDebugPSO();
	}
	CreateRootSignature();
    return CreatePSO();
}

void
Shutdown()
{
	effects::Shutdown();
	renderTexture.Release();
	ssilvbTarget.Release();
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
		if (!fxRootSig_Debug) CreateDebugRootSignature();
		if (!fxPSO_Debug) CreateDebugPSO();
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
ClearEffectsBuffers(DXGraphicsCommandList* cmdList)
{
	cmdList->ClearRenderTargetView(ssilvbTarget.RTV(0), CLEAR_VALUE, 0, nullptr);
}

void
AddTransitionsPrePostProcess(d3dx::D3D12ResourceBarrierList& barriers)
{
#if RENDER_GUI
	barriers.AddTransitionBarrier(renderTexture.Resource(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
#endif
	/*if (graphics::debug::RenderingSettings.VB_HalfRes)
	{
		barriers.AddTransitionBarrier(ssilvbTarget.Resource(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
	}*/
}

void
AddTransitionsMidPostProcess(d3dx::D3D12ResourceBarrierList& barriers)
{
}

void
AddTransitionsPostPostProcess(d3dx::D3D12ResourceBarrierList& barriers)
{
#if RENDER_GUI
	barriers.AddTransitionBarrier(renderTexture.Resource(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
#endif
}

void DoPostProcessing(DXGraphicsCommandList* cmdList, const D3D12FrameInfo& frameInfo, D3D12_CPU_DESCRIPTOR_HANDLE rtv, const D3D12Surface& surface)
{
	u32 ssilvbSrvIndex{ gpass::MotionVecBuffer().SRV().index };
	hlsl::PostProcessConstants* shaderParams{ core::CBuffer().AllocateSpace<hlsl::PostProcessConstants>() };
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
	//shaderParams->GPassMainBufferIndex = gpass::MainBuffer().SRV().index;
	shaderParams->GPassMainBufferIndex = resolve::GetResolveOutputSRV();
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
	shaderParams->DisplayAO = (u32)graphics::debug::RenderingSettings.DisplayAO;
	shaderParams->RenderGUI = (u32)graphics::debug::RenderingSettings.RenderGUI;
#endif
	for (auto [entity, dirLight] : ecs::scene::GetRO<ecs::component::DirectionalLight>())
	{
		shaderParams->SunDirection = dirLight.Direction;
		shaderParams->SunColor = dirLight.Color;
		break; // only the first directional light
	}

	//if (debug::RenderingSettings.VB_HalfRes)
	//{
	//	cmdList->RSSetViewports(1, effects::GetLowResViewport());
	//	cmdList->RSSetScissorRects(1, effects::GetLowResScissorRect());
	//	ClearEffectsBuffers(cmdList);

	//	cmdList->SetGraphicsRootSignature(ssilvbRootSig);
	//	cmdList->SetPipelineState(ssilvbPSO);

	//	using idx = FXRootParameterIndices;
	//	cmdList->SetGraphicsRootConstantBufferView(idx::GlobalShaderData, frameInfo.GlobalShaderData);
	//	cmdList->SetGraphicsRootConstantBufferView(idx::RootConstants, core::CBuffer().GpuAddress(shaderParams));

	//	graphics::debug::Settings::SSILVB_Settings* giSettings{ core::CBuffer().AllocateSpace<graphics::debug::Settings::SSILVB_Settings>() };
	//	memcpy(giSettings, &graphics::debug::RenderingSettings.SSILVB, sizeof(graphics::debug::Settings::SSILVB_Settings));
	//	cmdList->SetGraphicsRootConstantBufferView(idx::GISettings, core::CBuffer().GpuAddress(giSettings));

	//	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//	const D3D12_CPU_DESCRIPTOR_HANDLE target{ ssilvbTarget.RTV(0) };
	//	cmdList->OMSetRenderTargets(1, &target, 1, nullptr);
	//	cmdList->DrawInstanced(3, 1, 0, 0);

	//	d3dx::TransitionResource(ssilvbTarget.Resource(), cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	//	ssilvbSrvIndex = ssilvbTarget.SRV().index;
	//	if (graphics::debug::RenderingSettings.ApplyDualKawaseBlur)
	//	{
	//		effects::ApplyKawaseBlur(ssilvbTarget.Resource(), ssilvbTarget.SRV().index, cmdList);
	//		ssilvbSrvIndex = effects::GetBlurUpResultSrvIndex();
	//	}

	//	//FIXME: this not here
	//	cmdList->RSSetViewports(1, surface.Viewport());
	//	cmdList->RSSetScissorRects(1, surface.ScissorRect());
	//}

	//shaderParams->MiscBufferIndex = ssilvbSrvIndex;

#if IS_DLSS_ENABLED
	shaderParams->MiscBufferIndex = dlss::GetOutputBufferSRV().index;
#endif

	GT7ToneMapCurve* curveData{ core::CBuffer().AllocateSpace<GT7ToneMapCurve>() };
	memcpy(curveData, &_tonemapCurve, sizeof(GT7ToneMapCurve));

	cmdList->SetGraphicsRootSignature(fxRootSig);
	cmdList->SetPipelineState(fxPSO);

	using idx = FXRootParameterIndices;
	cmdList->SetGraphicsRootConstantBufferView(idx::GlobalShaderData, frameInfo.GlobalShaderData);
	cmdList->SetGraphicsRootConstantBufferView(idx::RootConstants, core::CBuffer().GpuAddress(shaderParams));
	cmdList->SetGraphicsRootConstantBufferView(idx::GTTonemapCurve, core::CBuffer().GpuAddress(curveData));
	graphics::debug::Settings::SSILVB_Settings* giSettings{ core::CBuffer().AllocateSpace<graphics::debug::Settings::SSILVB_Settings>() };
	memcpy(giSettings, &graphics::debug::RenderingSettings.SSILVB, sizeof(graphics::debug::Settings::SSILVB_Settings));
	cmdList->SetGraphicsRootConstantBufferView(idx::GISettings, core::CBuffer().GpuAddress(giSettings));

	if (fxRootSig == fxRootSig_Debug)
	{
		cmdList->SetGraphicsRoot32BitConstant(FXRootParameterIndices_Debug::DebugConstants, debug::GetDebugMode(), 0);
	}
	//cmdList->SetGraphicsRootDescriptorTable(idx::DescriptorTable, core::SrvHeap().GpuStart());

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
#if RENDER_GUI
	if (graphics::debug::RenderingSettings.RenderGUI)
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE imageSourceRTV{ renderTexture.RTV(0) };
		cmdList->OMSetRenderTargets(1, &imageSourceRTV, 1, nullptr);
		cmdList->DrawInstanced(3, 1, 0, 0);
	}
	else
	{
		cmdList->OMSetRenderTargets(1, &rtv, 1, nullptr);
		cmdList->DrawInstanced(3, 1, 0, 0);
	}
#else
	cmdList->OMSetRenderTargets(1, &rtv, 1, nullptr);
	cmdList->DrawInstanced(3, 1, 0, 0);
#endif
}

D3D12_GPU_DESCRIPTOR_HANDLE 
GetSrvGPUDescriptorHandle()
{
	return renderTexture.SRV().gpu;
}

D3D12_CPU_DESCRIPTOR_HANDLE GetPostProcessSurfaceRTV() { return renderTexture.RTV(0); }

void
ResetShaders(bool debug)
{
	if (!debug)
	{
		fxPSO_Default = nullptr;
		CreatePSO();
	}
	else
	{
		fxPSO_Debug = nullptr;
		CreateDebugPSO();
	}
}

}
