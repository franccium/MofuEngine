#include "D3D12DLSS.h"

#if IS_DLSS_ENABLED
#pragma comment(lib, "nvsdk_ngx_d.lib")

#include "DLSS/nvsdk_ngx.h"
#include "DLSS/nvsdk_ngx_defs_dlssd.h"
#include "DLSS/nvsdk_ngx_helpers_dlssd.h"
#include "DLSS/nvsdk_ngx_params_dlssd.h"
#include "../D3D12GPass.h"
#include "../D3D12ResolvePass.h"
#include "../D3D12Camera.h"

namespace mofu::graphics::d3d12::dlss {
namespace {
constexpr const char* ProjectID{ "c4f4d1d1-1000-4872-511a-44b67d3b45ca" };
constexpr const char* EngineVersion{ "0.0.1" };
constexpr const wchar_t* NGXDataPath{ L"NGXData" };
constexpr const wchar_t* DLSS_DLL_PATH{ L"" };

NVSDK_NGX_Handle* _dlssHandle{ nullptr };

u32v2 _optimalResolution{};
u32 _jitterX{ 1 };
u32 _jitterY{ 1 };

NVSDK_NGX_Parameter* _ngxParameters{ nullptr };

struct DLSSOutputBuffer
{
	DXResource* Buffer{ nullptr };
	DescriptorHandle UAV{};
	DescriptorHandle SRV{};
	u32v2 TargetResolution{};
};
DLSSOutputBuffer _outputBuffer{};
constexpr f32 CLEAR_VALUE[4]{ 0.f, 0.f, 0.f, 0.f };

bool _isInitialized{ false };

struct
{
	NVSDK_NGX_PerfQuality_Value DLSSQuality{ NVSDK_NGX_PerfQuality_Value::NVSDK_NGX_PerfQuality_Value_MaxQuality };
} DLSSSettings;

void
CreateOutputBuffer(u32v2 dimensions)
{
	core::Release(_outputBuffer.Buffer);
	_outputBuffer.TargetResolution = dimensions;

	D3D12_RESOURCE_DESC1 desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = dimensions.x;
	desc.Height = dimensions.y;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 0;
	desc.Format = gpass::MAIN_BUFFER_FORMAT;
	desc.SampleDesc = { MSAA_SAMPLE_COUNT, MSAA_SAMPLE_QUALITY };
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	desc.SamplerFeedbackMipRegion = { 0, 0, 0 };

	DXDevice* const device{ core::Device() };
	DXCall(device->CreateCommittedResource2(&d3dx::HeapProperties.DEFAULT_HEAP,
		D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, nullptr, IID_PPV_ARGS(&_outputBuffer.Buffer)));
	NAME_D3D12_OBJECT(_outputBuffer.Buffer, L"DLSS Output Buffer");

	_outputBuffer.UAV = core::UavHeap().AllocateDescriptor();
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = gpass::MAIN_BUFFER_FORMAT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	uavDesc.Texture2D.PlaneSlice = 0;
	device->CreateUnorderedAccessView(_outputBuffer.Buffer, nullptr, &uavDesc, _outputBuffer.UAV.cpu);

	_outputBuffer.SRV = core::SrvHeap().AllocateDescriptor();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = gpass::MAIN_BUFFER_FORMAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	device->CreateShaderResourceView(_outputBuffer.Buffer, &srvDesc, _outputBuffer.SRV.cpu);
}

} // anonymous namespace

void
SetTargetResolution(u32v2 size)
{
	if(_outputBuffer.TargetResolution.x != size.x || _outputBuffer.TargetResolution.y != size.y)
	{ 
		CreateOutputBuffer(size);
		assert(_outputBuffer.Buffer);
	}
}

bool 
Initialize()
{
	if (!DLSS_ENABLED || _isInitialized) return true; // FIXME: dont call this every frame
	_isInitialized = true;
	NVSDK_NGX_PathListInfo pathListInfo{};
	pathListInfo.Length = 1;
	pathListInfo.Path = &DLSS_DLL_PATH;
	NVSDK_NGX_FeatureCommonInfo featureInfo{ pathListInfo };
	NVSDK_NGX_Result result{ NVSDK_NGX_Result_Success };

	result = NVSDK_NGX_D3D12_Init_with_ProjectID(ProjectID, NVSDK_NGX_EngineType::NVSDK_NGX_ENGINE_TYPE_CUSTOM,
		EngineVersion, NGXDataPath, core::Device(), &featureInfo);
	if (result != NVSDK_NGX_Result_Success) return false;

	i32 isDLSSSupported{ 0 };
	i32 needsUpdatedDriver{ 0 };
	u32 minDriverVerMajor{ 0 };
	u32 minDriverVerMinor{ 0 };
	bool destroyCapabilityParameters{ true };

	result = NVSDK_NGX_D3D12_GetCapabilityParameters(&_ngxParameters);
	if (result != NVSDK_NGX_Result_Success)
	{
		NVSDK_NGX_D3D12_DestroyParameters(_ngxParameters);
		return false;
	}

	NVSDK_NGX_Result resUpdatedDriver{ _ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver) };
	NVSDK_NGX_Result ResultMinDriverVersionMajor{ _ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &minDriverVerMajor) };
	NVSDK_NGX_Result ResultMinDriverVersionMinor{ _ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &minDriverVerMinor) };

	if (NVSDK_NGX_SUCCEED(resUpdatedDriver))
	{
		if (needsUpdatedDriver && NVSDK_NGX_SUCCEED(ResultMinDriverVersionMajor) && NVSDK_NGX_SUCCEED(ResultMinDriverVersionMinor))
		{
			//  Min Driver Version required: minDriverVersionMajor.minDriverVersionMinor
			NVSDK_NGX_D3D12_DestroyParameters(_ngxParameters);
			return false;
		}
	}

	NVSDK_NGX_Result ResultDlssSupported{ _ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &isDLSSSupported) };
	if (NVSDK_NGX_FAILED(ResultDlssSupported) || !isDLSSSupported)
	{
		NVSDK_NGX_D3D12_DestroyParameters(_ngxParameters);
		return false;
	}

	ResultDlssSupported = _ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, &isDLSSSupported);
	if (NVSDK_NGX_FAILED(ResultDlssSupported) || !isDLSSSupported)
	{
		NVSDK_NGX_D3D12_DestroyParameters(_ngxParameters);
		return false;
	}

	return CreateDLSSFeature();
}

bool 
CreateDLSSFeature()
{
	f32 Sharpness;
	NVSDK_NGX_PerfQuality_Value perfQualityValue{ DLSSSettings.DLSSQuality };
	u32 renderOptimalWidth;
	u32 renderOptimalHeight;
	u32 renderMaxWidth;
	u32 renderMaxHeight;
	u32 renderMinWidth;
	u32 renderMinHeight;

	NVSDK_NGX_Result result{ NGX_DLSS_GET_OPTIMAL_SETTINGS(_ngxParameters, graphics::DEFAULT_WIDTH, graphics::DEFAULT_HEIGHT, perfQualityValue,
		&renderOptimalWidth, &renderOptimalHeight, &renderMaxWidth, &renderMaxHeight, &renderMinWidth, &renderMinHeight, &Sharpness) };
	if (renderOptimalWidth == 0 || renderOptimalHeight == 0)
	{
		// the selected PerfQuality mode has not been made available yet, need to select another one
		NVSDK_NGX_D3D12_DestroyParameters(_ngxParameters);
		return false;
	}

	u32 flags{ NVSDK_NGX_DLSS_Feature_Flags_MVLowRes | NVSDK_NGX_DLSS_Feature_Flags_MVJittered | NVSDK_NGX_DLSS_Feature_Flags_DepthInverted };
	NVSDK_NGX_DLSS_Feature_Flags dlssFlags{ (NVSDK_NGX_DLSS_Feature_Flags)flags };
	NVSDK_NGX_Feature_Create_Params createParams{};
	createParams.InWidth = renderOptimalWidth;
	createParams.InHeight = renderOptimalHeight;
	createParams.InTargetWidth = graphics::DEFAULT_WIDTH;
	createParams.InTargetHeight = graphics::DEFAULT_HEIGHT;
	createParams.InPerfQualityValue = perfQualityValue;

	NVSDK_NGX_DLSS_Create_Params dlssParams{ createParams, dlssFlags };
	result = NGX_D3D12_CREATE_DLSS_EXT(core::GraphicsCommandList(), 1, 1, &_dlssHandle, _ngxParameters, &dlssParams);
	if (result != NVSDK_NGX_Result_Success)
	{
		NVSDK_NGX_D3D12_DestroyParameters(_ngxParameters);
		return false;
	}

	_optimalResolution = { renderOptimalWidth, renderOptimalHeight };
}

void Shutdown()
{
	core::SrvHeap().FreeDescriptor(_outputBuffer.SRV);
	core::UavHeap().FreeDescriptor(_outputBuffer.UAV);
	core::Release(_outputBuffer.Buffer);
	NVSDK_NGX_D3D12_ReleaseFeature(_dlssHandle);
	NVSDK_NGX_D3D12_DestroyParameters(_ngxParameters);
	NVSDK_NGX_D3D12_Shutdown1(core::Device());
}

void 
DoDLSSPass(DXResource* colorBuffer, DXResource* outputBuffer, const camera::D3D12Camera& camera, DXGraphicsCommandList* const cmdList)
{
	const bool shouldResetDLSS{ false };

	NVSDK_NGX_D3D12_DLSS_Eval_Params evalParams{};
	evalParams.Feature.pInColor = colorBuffer;
	evalParams.Feature.pInOutput = _outputBuffer.Buffer;
	evalParams.pInDepth = gpass::DepthBuffer().Resource();
	evalParams.pInMotionVectors = gpass::MotionVecBuffer().Resource();
	evalParams.InJitterOffsetX = camera.CurrentJitter().x * 0.5f;
	evalParams.InJitterOffsetY = camera.CurrentJitter().y * 0.5f;
	evalParams.InRenderSubrectDimensions = { _optimalResolution.x, _optimalResolution.y };
	evalParams.InReset = shouldResetDLSS;
	evalParams.InMVScaleX = _outputBuffer.TargetResolution.x;
	evalParams.InMVScaleY = _outputBuffer.TargetResolution.y;
#ifndef _DEBUG
	TODO_("Relase build memory exceptions");
#endif
	NVSDK_NGX_Result result{ NGX_D3D12_EVALUATE_DLSS_EXT(cmdList, _dlssHandle, _ngxParameters, &evalParams) };
}

u32v2 GetOptimalResolution()
{
	return _optimalResolution;
}

void
ApplyBarriersBeforeDLSS(d3dx::D3D12ResourceBarrierList& barriers)
{
	barriers.AddTransitionBarrier(_outputBuffer.Buffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void
ApplyBarriersAfterDLSS(d3dx::D3D12ResourceBarrierList& barriers)
{
	barriers.AddTransitionBarrier(_outputBuffer.Buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

DescriptorHandle GetOutputBufferSRV()
{
	return _outputBuffer.SRV;
}

}
#endif