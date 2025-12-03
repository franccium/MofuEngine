#include "D3D12KawaseBlur.h"
#include "Graphics/D3D12/D3D12Shaders.h"
#include "Graphics/D3D12/D3D12Core.h"
#include "Graphics/D3D12/D3D12Surface.h"

namespace mofu::graphics::d3d12::effects {
namespace {

D3D12_VIEWPORT _halfResViewport{};
D3D12_VIEWPORT _quarterResViewport{};
D3D12_RECT _halfResScissor{};
D3D12_RECT _quarterResScissor{};


ID3D12RootSignature* _rootSig{ nullptr };
ID3D12PipelineState* _psoUp{ nullptr };
ID3D12PipelineState* _psoDown{ nullptr };

u32v2 downDimensions{ graphics::DEFAULT_WIDTH / 4, graphics::DEFAULT_HEIGHT / 4 };
D3D12RenderTexture downBuffer{};
u32v2 upDimensions{ graphics::DEFAULT_WIDTH / 2, graphics::DEFAULT_HEIGHT / 2 };
D3D12RenderTexture upBuffer{};
DXGI_FORMAT BUFFER_FORMAT{ DXGI_FORMAT_R16G16B16A16_FLOAT };
constexpr f32 CLEAR_VALUE[4]{ 0.f, 0.f, 0.f, 0.f };

void
CreateRootSignature()
{
	assert(!_rootSig);

	d3dx::D3D12DescriptorRange range
	{
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND, 0, 0,
		D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE
	};

	d3dx::D3D12RootParameter parameters[1]{};
	parameters[0].AsConstants(1, D3D12_SHADER_VISIBILITY_PIXEL, 0);

	struct D3D12_STATIC_SAMPLER_DESC samplers[]
	{
		d3dx::StaticSampler(d3dx::SamplerState.STATIC_LINEAR_CLAMP, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL),
	};

	d3dx::D3D12RootSignatureDesc rootSigDesc
	{
		&parameters[0], _countof(parameters), d3dx::D3D12RootSignatureDesc::DEFAULT_FLAGS,
		&samplers[0], _countof(samplers)
	};

	rootSigDesc.Flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	_rootSig = rootSigDesc.Create();
	assert(_rootSig);
	NAME_D3D12_OBJECT(_rootSig, L"Kawase Blur Root Signature");
}

bool
CreatePSO()
{
	assert(!_psoDown && !_psoUp);
	constexpr u64 alignedStreamSize{ math::AlignUp<sizeof(u64)>(sizeof(d3dx::D3D12PipelineStateSubobjectStream)) };
	u8* const streamPtr{ (u8*)_alloca(alignedStreamSize) };
	ZeroMemory(streamPtr, alignedStreamSize);
	new (streamPtr) d3dx::D3D12PipelineStateSubobjectStream{};
	d3dx::D3D12PipelineStateSubobjectStream& stream{ *(d3dx::D3D12PipelineStateSubobjectStream* const)streamPtr };

	stream.rootSignature = _rootSig;
	stream.vs = shaders::GetEngineShader(EngineShader::FullscreenTriangleVS);
	stream.ps = shaders::GetEngineShader(EngineShader::KawaseBlurDownPS);
	stream.primitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	stream.rasterizer = d3dx::RasterizerState.NO_CULLING;

	D3D12_RT_FORMAT_ARRAY rtfArray{};
	rtfArray.NumRenderTargets = 1;
	rtfArray.RTFormats[0] = D3D12Surface::DEFAULT_BACK_BUFFER_FORMAT;
	stream.renderTargetFormats = rtfArray;

	_psoDown = d3dx::CreatePipelineState(&stream, sizeof(stream));
	NAME_D3D12_OBJECT(_psoDown, L"Kawase Blur PSO");

	stream.ps = shaders::GetEngineShader(EngineShader::KawaseBlurUpPS);
	_psoUp = d3dx::CreatePipelineState(&stream, sizeof(stream));
	NAME_D3D12_OBJECT(_psoUp, L"Kawase Blur PSO");

	return _psoDown && _psoUp;
}

}

void
CreateFXBuffers(u32v2 renderDimensions)
{
	downBuffer.Release();
	upBuffer.Release();
	downDimensions = { renderDimensions.x / 2, renderDimensions.y / 2 };
	upDimensions = { renderDimensions.x, renderDimensions.y };

	D3D12_RESOURCE_DESC1 desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = downDimensions.x;
	desc.Height = downDimensions.y;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.SampleDesc = { MSAA_SAMPLE_COUNT, MSAA_SAMPLE_QUALITY };
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	desc.SamplerFeedbackMipRegion = { 0, 0, 0 };
	{
		D3D12TextureInitInfo texInfo;
		texInfo.desc = &desc;
		texInfo.initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		texInfo.clearValue.Format = desc.Format;
		memcpy(&texInfo.clearValue.Color, &CLEAR_VALUE[0], sizeof(CLEAR_VALUE));
		downBuffer = D3D12RenderTexture{ texInfo };
	}
	NAME_D3D12_OBJECT(downBuffer.Resource(), L"Kawase Blur Down Buffer");

	desc.Width = upDimensions.x;
	desc.Height = upDimensions.y;
	{
		D3D12TextureInitInfo texInfo;
		texInfo.desc = &desc;
		texInfo.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		texInfo.clearValue.Format = desc.Format;
		memcpy(&texInfo.clearValue.Color, &CLEAR_VALUE[0], sizeof(CLEAR_VALUE));
		upBuffer = D3D12RenderTexture{ texInfo };
	}
	NAME_D3D12_OBJECT(upBuffer.Resource(), L"Kawase Blur Up Buffer");

	assert(downBuffer.Resource() && upBuffer.Resource());

	_halfResViewport.TopLeftX = 0.f;
	_halfResViewport.TopLeftY = 0.f;
	_halfResViewport.Width = (float)upDimensions.x;
	_halfResViewport.Height = (float)upDimensions.y;
	_halfResViewport.MinDepth = 0.f;
	_halfResViewport.MaxDepth = 1.f;

	_quarterResViewport.TopLeftX = 0.f;
	_quarterResViewport.TopLeftY = 0.f;
	_quarterResViewport.Width = (float)downDimensions.x;
	_quarterResViewport.Height = (float)downDimensions.y;
	_quarterResViewport.MinDepth = 0.f;
	_quarterResViewport.MaxDepth = 1.f;

	_halfResScissor = { 0, 0, (i32)upDimensions.x, (i32)upDimensions.y };
	_quarterResScissor = { 0, 0, (i32)downDimensions.x, (i32)downDimensions.y };
}

void 
Initialize()
{
	CreateRootSignature();
	assert(_rootSig);
	CreatePSO();
	assert(_psoDown && _psoUp);
}

void
CreatePSOs()
{
	core::Release(_psoDown);
	core::Release(_psoUp);
	CreatePSO();
	assert(_psoDown && _psoUp);
}

void
Shutdown()
{
	downBuffer.Release();
	upBuffer.Release();
	core::Release(_psoDown);
	core::Release(_psoUp);
	core::Release(_rootSig);
}

u32 GetBlurUpResultSrvIndex()
{
	return upBuffer.SRV().index;
}

const D3D12_VIEWPORT* GetLowResViewport()
{
	return &_halfResViewport;
}

const D3D12_RECT* GetLowResScissorRect()
{
	return &_halfResScissor;
}

void 
ApplyKawaseBlur(DXResource* res, u32 texSrvIndex, DXGraphicsCommandList* const cmdList)
{
	assert(downBuffer.Resource() && upBuffer.Resource());

	d3dx::TransitionResource(upBuffer.Resource(), cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmdList->RSSetViewports(1, &_quarterResViewport);
	cmdList->RSSetScissorRects(1, &_quarterResScissor);
	cmdList->ClearRenderTargetView(downBuffer.RTV(0), CLEAR_VALUE, 0, nullptr);
	cmdList->ClearRenderTargetView(upBuffer.RTV(0), CLEAR_VALUE, 0, nullptr);
	cmdList->SetGraphicsRootSignature(_rootSig);

	{
		cmdList->SetPipelineState(_psoDown);
		cmdList->SetGraphicsRoot32BitConstant(0, texSrvIndex, 0);

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		const D3D12_CPU_DESCRIPTOR_HANDLE target{ downBuffer.RTV(0) };
		cmdList->OMSetRenderTargets(1, &target, 1, nullptr);
		cmdList->DrawInstanced(3, 1, 0, 0);
	}

	//d3dx::TransitionResource(res, cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	d3dx::TransitionResource(downBuffer.Resource(), cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	cmdList->RSSetViewports(1, &_halfResViewport);
	cmdList->RSSetScissorRects(1, &_halfResScissor);

	{
		cmdList->SetPipelineState(_psoUp);
		cmdList->SetGraphicsRoot32BitConstant(0, downBuffer.SRV().index, 0);

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		const D3D12_CPU_DESCRIPTOR_HANDLE target{ upBuffer.RTV(0) }; //TODO: render to the input texture?
		cmdList->OMSetRenderTargets(1, &target, 1, nullptr);
		cmdList->DrawInstanced(3, 1, 0, 0);
	}

	d3dx::TransitionResource(downBuffer.Resource(), cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	d3dx::TransitionResource(upBuffer.Resource(), cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

}