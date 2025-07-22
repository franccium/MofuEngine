#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12 {
struct D3D12FrameInfo;
}

namespace mofu::graphics::d3d12::gpass {
struct GPassCache;

constexpr DXGI_FORMAT MAIN_BUFFER_FORMAT{ DXGI_FORMAT_R16G16B16A16_FLOAT };
constexpr DXGI_FORMAT DEPTH_BUFFER_FORMAT{ DXGI_FORMAT_D32_FLOAT };

struct OpaqueRootParameters
{
	enum Parameter : u32
	{
		GlobalShaderData,
		PerObjectData,
		PositionBuffer,
		ElementBuffer,
		SrvIndices,
		DirectionalLights,
		CullableLights,
		LightGrid,
		LightIndexList,

		Count
	};
};

bool Initialize();
void Shutdown();

[[nodiscard]] const D3D12RenderTexture& MainBuffer();
[[nodiscard]] const D3D12DepthBuffer& DepthBuffer();

// needs to be called every frame to check whether the buffers should be resized in case of resizing the biggest window
bool CreateBuffers(u32v2 size);
void SetBufferSize(u32v2 size);

void DoDepthPrepass(DXGraphicsCommandList* const* cmdLists, const D3D12FrameInfo& info, u32 firstWorker);
void Render(DXGraphicsCommandList* cmdList, const D3D12FrameInfo& info);
void RenderMT(DXGraphicsCommandList* const* cmdLists, const D3D12FrameInfo& info);

void AddTransitionsForDepthPrepass(d3dx::D3D12ResourceBarrierList& barriers);
void AddTransitionsForGPass(d3dx::D3D12ResourceBarrierList& barriers);
void AddTransitionsForPostProcess(d3dx::D3D12ResourceBarrierList& barriers);

void ClearDepthStencilView(DXGraphicsCommandList* cmdList);
void ClearMainBufferView(DXGraphicsCommandList* cmdList);
void SetRenderTargetsForDepthPrepass(DXGraphicsCommandList* cmdList);
void SetRenderTargetsForGPass(DXGraphicsCommandList* cmdList);

D3D12FrameInfo& GetCurrentD3D12FrameInfo();
void StartNewFrame(const D3D12FrameInfo& frameInfo);

GPassCache& GetGPassFrameCache();
}