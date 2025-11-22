#include "SSSR.h"
#include "Utilities/Logger.h"

#include "Graphics/D3D12/D3D12GPass.h"
#include "Graphics/D3D12/D3D12Surface.h"
#include "Graphics/D3D12/D3D12Camera.h"
#include "Graphics/D3D12/D3D12Content/D3D12Texture.h"
#include "Graphics/Lights/Light.h"
#include "Graphics/RenderingDebug.h"
#include "Graphics/D3D12/D3D12ResourceWatch.h"

namespace mofu::graphics::d3d12::ffx::sssr {
namespace {
constexpr u32 FFX_CONTEXT_COUNT{ 1 };
constexpr FfxSurfaceFormat FFX_SSSR_SURFACE_FORMAT{ FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT };

FfxSssrContext _context;
FfxInterface _backendInterface;
u8* _scratchMem{ nullptr };
FfxSssrDispatchDescription _dispatchDesc{};

D3D12RenderTexture outputBuffer{};
constexpr f32 CLEAR_VALUE[4]{ 0.f, 0.f, 0.f, 0.f };

}

bool
CreateBuffers(u32v2 size)
{
	assert(size.x && size.y);

	outputBuffer.Release();

	D3D12_RESOURCE_DESC1 desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = size.x;
	desc.Height = size.y;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = SSSR_OUTPUT_FORMAT;
	desc.SampleDesc = { MSAA_SAMPLE_COUNT, MSAA_SAMPLE_QUALITY };
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	desc.SamplerFeedbackMipRegion = { 0, 0, 0 };
	{
		D3D12TextureInitInfo texInfo;
		texInfo.desc = &desc;
		texInfo.initialState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		texInfo.clearValue.Format = desc.Format;
		memcpy(&texInfo.clearValue.Color, &CLEAR_VALUE[0], sizeof(CLEAR_VALUE));
		outputBuffer = D3D12RenderTexture{ texInfo };
	}
	NAME_D3D12_OBJECT(outputBuffer.Resource(), L"SSSR Output Buffer");

	const DXResource* const output{ outputBuffer.Resource() };
	assert(output);
	_dispatchDesc.output = ffxGetResourceDX12(output, ffxGetResourceDescriptionDX12(output), L"SSSR Output", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);

	return output;
}

void 
Initialize()
{
#if !IS_SSSR_ENABLED
	return;
#endif
	assert(core::Device());

	const u64 scratchMemSize{ ffxGetScratchMemorySizeDX12(FFX_CONTEXT_COUNT) };
	_scratchMem = (u8*)_aligned_malloc(scratchMemSize, 32);
	memset(_scratchMem, 0, scratchMemSize);
	FfxDevice device{ ffxGetDeviceDX12(core::Device()) };
	FfxErrorCode err{ ffxGetInterfaceDX12(&_backendInterface, ffxGetDeviceDX12(core::Device()), _scratchMem, scratchMemSize, FFX_CONTEXT_COUNT) };

	if (err != FFX_OK)
	{
		_aligned_free(_scratchMem);
		assert(false);
		log::Error("FFX SSSR: Failed to get DX12 interface");
		return;
	}

	FfxSssrContextDescription desc{};
	desc.flags = FFX_SSSR_ENABLE_DEPTH_INVERTED;
	if constexpr (DLSS_ENABLED)
	{
		assert(false);
	}
	else
	{
		desc.renderSize = { graphics::DEFAULT_WIDTH, graphics::DEFAULT_HEIGHT };
	}
	desc.normalsHistoryBufferFormat = FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT;
	desc.backendInterface = _backendInterface;
	ffxSssrContextCreate(&_context, &desc);

	bool success{ CreateBuffers({ graphics::DEFAULT_WIDTH, graphics::DEFAULT_HEIGHT }) };
	assert(success);
	if (!success)
	{
		Shutdown();
	}
}



void 
Shutdown()
{
	if (_scratchMem)
	{
		ffxSssrContextDestroy(&_context);
		_aligned_free(_scratchMem);
	}
}

void
GatherResources()
{
#if !IS_SSSR_ENABLED
	return;
#endif
	_dispatchDesc.commandList = ffxGetCommandListDX12(core::GraphicsCommandList());

	if (resources::WasResourceUpdated(resources::ResourceUpdateState::GPassBuffers))
	{
		const DXResource* const colorBuffer{ gpass::MainBuffer().Resource() };
		assert(colorBuffer);
		_dispatchDesc.color = ffxGetResourceDX12(colorBuffer, ffxGetResourceDescriptionDX12(colorBuffer), L"Color Buffer", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);

		const DXResource* const depthBuffer{ gpass::DepthBuffer().Resource() };
		assert(depthBuffer);
		_dispatchDesc.depth = ffxGetResourceDX12(depthBuffer, ffxGetResourceDescriptionDX12(depthBuffer), L"Depth Buffer", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);

		const DXResource* const motionVectors{ gpass::MotionVecBuffer().Resource() };
		assert(motionVectors);
		_dispatchDesc.motionVectors = ffxGetResourceDX12(motionVectors, ffxGetResourceDescriptionDX12(motionVectors), L"Motion Vectors Buffer", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
		_dispatchDesc.motionVectorScale = { 0.5f, -0.5f };

		const DXResource* const normalBuffer{ gpass::NormalBuffer().Resource() };
		assert(normalBuffer);
		_dispatchDesc.normal = ffxGetResourceDX12(normalBuffer, ffxGetResourceDescriptionDX12(normalBuffer), L"Color Buffer", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
		_dispatchDesc.normalUnPackMul = 1.f;
		_dispatchDesc.normalUnPackAdd = 0.f;

		const DXResource* const matPropsBuffer{ gpass::MaterialPropertiesBuffer().Resource() };
		_dispatchDesc.materialParameters = ffxGetResourceDX12(matPropsBuffer, ffxGetResourceDescriptionDX12(matPropsBuffer), L"Material Parameters Buffer", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
		_dispatchDesc.roughnessChannel = 0;
		_dispatchDesc.isRoughnessPerceptual = false;
	}

	graphics::debug::Settings::FFX_SSSR_Settings& settings{ graphics::debug::RenderingSettings.FFX_SSSR };
	if (settings.WasAmbientLightChanged)
	{
		const DXResource* const environmentMap{ content::texture::GetResource(graphics::light::GetLightSet(0).EnvironmentMapTextureID) };
		assert(environmentMap);
		_dispatchDesc.environmentMap = ffxGetResourceDX12(environmentMap, ffxGetResourceDescriptionDX12(environmentMap), L"Environment Map", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);

		const DXResource* const brdfTexture{ content::texture::GetResource(graphics::light::GetLightSet(0).BrdfLutTextureID) };
		assert(brdfTexture);
		_dispatchDesc.brdfTexture = ffxGetResourceDX12(brdfTexture, ffxGetResourceDescriptionDX12(brdfTexture), L"BRDF Texture", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
		settings.WasAmbientLightChanged = false;
	}

	_dispatchDesc.renderSize = { graphics::DEFAULT_WIDTH, graphics::DEFAULT_HEIGHT };
	_dispatchDesc.depthBufferThickness = settings.DepthBufferThickness;
	_dispatchDesc.roughnessThreshold = settings.RoughnessThreshold;
	_dispatchDesc.iblFactor = settings.IBLFactor;
	_dispatchDesc.temporalStabilityFactor = settings.TemporalStabilityFactor;
	_dispatchDesc.varianceThreshold = settings.VarianceThreshold;
	_dispatchDesc.maxTraversalIntersections = settings.MaxTraversalIntersections;
	_dispatchDesc.minTraversalOccupancy = settings.MinTraversalOccupancy;
	_dispatchDesc.mostDetailedMip = settings.MostDetailedMip;
	_dispatchDesc.samplesPerQuad = settings.SamplesPerQuad;
	_dispatchDesc.temporalVarianceGuidedTracingEnabled = true;
}

const D3D12RenderTexture& ReflectionsBuffer()
{
	return outputBuffer;
}

void 
Dispatch(const D3D12FrameInfo& frameInfo)
{
#if !IS_SSSR_ENABLED
	return;
#endif
	_dispatchDesc.commandList = ffxGetCommandListDX12(core::GraphicsCommandList());

	GatherResources(); // TODO: for now its here

	// camera matrices in column major
	const camera::D3D12Camera* camera{ frameInfo.Camera };
	DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)_dispatchDesc.invViewProjection, camera->InverseViewProjection());
	DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)_dispatchDesc.projection, camera->Projection());
	DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)_dispatchDesc.invProjection, camera->InverseProjection());
	DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)_dispatchDesc.view, camera->View());
	DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)_dispatchDesc.invView, camera->InverseView());
	memcpy(_dispatchDesc.prevViewProjection, camera->PrevViewProjection(), sizeof(m4x4));

	FfxErrorCode err{ ffxSssrContextDispatch(&_context, &_dispatchDesc) };
	if (err != FFX_OK)
	{
		assert(false);
		log::Error("FFX SSSR: Dispatch failed");
	}

}

}