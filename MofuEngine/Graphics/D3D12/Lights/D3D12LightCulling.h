#pragma once
#include "../D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::light {
constexpr u32 LIGHT_CULLING_TILE_SIZE{ 32 };

bool InitializeLightCulling();
void ShutdownLightCulling();

void CullLights(DXGraphicsCommandList* const cmdList, const D3D12FrameInfo& d3d12FrameInfo, d3dx::D3D12ResourceBarrierList& barriers);
id_t AddLightCuller();
void RemoveLightCuller(id_t cullerID);

D3D12_GPU_VIRTUAL_ADDRESS GetGridFrustumsBuffer(u32 lightCullingID, u32 frameIndex);
D3D12_GPU_VIRTUAL_ADDRESS GetLightGridOpaqueBuffer(u32 lightCullingID, u32 frameIndex);
D3D12_GPU_VIRTUAL_ADDRESS GetLightIndexListOpaqueBuffer(u32 lightCullingID, u32 frameIndex);
}