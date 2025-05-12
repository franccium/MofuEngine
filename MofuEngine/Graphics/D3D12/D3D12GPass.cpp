#include "D3D12GPass.h"
#include "D3D12Shaders.h"

namespace mofu::graphics::d3d12::gpass {
namespace {

constexpr math::u32v2 INITIAL_DIMENSIONS{ 100, 100 };

D3D12RenderTexture gpassMainBuffer{};
D3D12DepthBuffer gpassDepthBuffer{};
math::u32v2 dimensions{};

ID3D12RootSignature* gpassRootSig{ nullptr };
ID3D12PipelineState* gpassPSO{ nullptr };

#if _DEBUG
constexpr f32 CLEAR_VALUE[4]{ 0.5f, 0.5f, 0.5f, 0.5f };
#else
constexpr f32 CLEAR_VALUE[4]{};
#endif

bool
CreateGPassPSO()
{
	assert(!gpassRootSig && !gpassPSO);

	d3dx::D3D12RootParameter parameters[1]{};
	parameters[OpaqueRootParameters::GlobalShaderData].AsConstants(3, D3D12_SHADER_VISIBILITY_PIXEL, 1);
	//gpassRootSig = d3dx::D3D12RootSignatureDesc{ &parameters[0], 1 }.Create();
	d3dx::D3D12RootSignatureDesc rootSigDesc{ &parameters[0], 1 };
	rootSigDesc.Flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
	gpassRootSig = rootSigDesc.Create();
	NAME_D3D12_OBJECT(gpassRootSig, L"GPass Root Signature");

	struct {
		d3dx::D3D12PipelineStateSubobjectRootSignature rootSignature{ gpassRootSig };
		d3dx::D3D12PipelineStateSubobjectVS vs{ shaders::GetEngineShader(shaders::EngineShader::FullscreenTriangleVS) };
		d3dx::D3D12PipelineStateSubobjectPS ps{ shaders::GetEngineShader(shaders::EngineShader::ColorFillPS) };
		d3dx::D3D12PipelineStateSubobjectPrimitiveTopology primitiveTopology{ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE };
		d3dx::D3D12PipelineStateSubobjectRenderTargetFormats renderTargetFormats{};
		d3dx::D3D12PipelineStateSubobjectDepthStencilFormat depthStencilFormat{ DEPTH_BUFFER_FORMAT };
		d3dx::D3D12PipelineStateSubobjectRasterizer rasterizer{ d3dx::RasterizerState.NO_CULLING };
		d3dx::D3D12PipelineStateSubobjectDepthStencil depthStencil{ d3dx::DepthState.DISABLED };
	} stream;

	D3D12_RT_FORMAT_ARRAY rtfArray{};
	rtfArray.NumRenderTargets = 1;
	rtfArray.RTFormats[0] = MAIN_BUFFER_FORMAT;
	stream.renderTargetFormats = rtfArray;
	gpassPSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
	NAME_D3D12_OBJECT(gpassPSO, L"GPass PSO");

	return gpassRootSig && gpassPSO;
}

} // anonymous namespace

bool
Initialize()
{
	return CreateGPassPSO() && CreateBuffers(INITIAL_DIMENSIONS);
}

void 
Shutdown()
{
	gpassMainBuffer.Release();
	gpassDepthBuffer.Release();
	dimensions = INITIAL_DIMENSIONS;
}

bool 
CreateBuffers(math::u32v2 size)
{
	assert(size.x && size.y);

	gpassDepthBuffer.Release();
	gpassDepthBuffer.Release();
	dimensions = size;

	// Create the main buffer
	D3D12_RESOURCE_DESC1 desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0; // 64KB (or 4MB for multi-sampled textures)
	desc.Width = size.x;
	desc.Height = size.y;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 0;
	desc.Format = MAIN_BUFFER_FORMAT;
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

	return gpassMainBuffer.Resource() && gpassDepthBuffer.Resource();
}

void 
SetBufferSize(math::u32v2 size)
{
	math::u32v2& d{ dimensions };
	if (size.x > d.x || size.y > d.y)
	{
		d = { std::max(size.x, d.x), std::max(size.y, d.y) };
		CreateBuffers(size);
		assert(gpassMainBuffer.Resource() && gpassDepthBuffer.Resource());
	}
}

void 
DoDepthPrepass(DXGraphicsCommandList* cmdList, const D3D12FrameInfo& info)
{
	
}

void 
Render(DXGraphicsCommandList* cmdList, const D3D12FrameInfo& info)
{
	cmdList->SetGraphicsRootSignature(gpassRootSig);
	cmdList->SetPipelineState(gpassPSO);

	static u32 frame;
	struct
	{
		f32 width;
		f32 height;
		u32 frame;
	} constants{ (f32)info.surfaceWidth, (f32)info.surfaceHeight, ++frame };

	cmdList->SetGraphicsRoot32BitConstants(OpaqueRootParameters::GlobalShaderData, 3, &constants, 0);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(3, 1, 0, 0);
}

void 
AddTransitionsForDepthPrepass(d3dx::D3D12ResourceBarrierList& barriers)
{
	barriers.AddTransitionBarrier(gpassMainBuffer.Resource(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY);

	barriers.AddTransitionBarrier(gpassDepthBuffer.Resource(),
		D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void 
AddTransitionsForGPass(d3dx::D3D12ResourceBarrierList& barriers)
{
	barriers.AddTransitionBarrier(gpassMainBuffer.Resource(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY);

	barriers.AddTransitionBarrier(gpassDepthBuffer.Resource(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void 
AddTransitionsForPostProcess(d3dx::D3D12ResourceBarrierList& barriers)
{
	barriers.AddTransitionBarrier(gpassMainBuffer.Resource(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void 
SetRenderTargetsForDepthPrepass(DXGraphicsCommandList* cmdList)
{
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv{ gpassDepthBuffer.Dsv() };
	cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.f, 0, 0, nullptr);
	cmdList->OMSetRenderTargets(0, nullptr, 0, &dsv);
}

void 
SetRenderTargetsForGPass(DXGraphicsCommandList* cmdList)
{
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv{ gpassDepthBuffer.Dsv() };
	const D3D12_CPU_DESCRIPTOR_HANDLE rtv{ gpassMainBuffer.Rtv(0) };
	cmdList->ClearRenderTargetView(rtv, CLEAR_VALUE, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &rtv, 0, &dsv);
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

}
