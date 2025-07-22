#pragma once
#include "../D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12 {

}

namespace mofu::graphics::d3d12::light {

void CullLights(DXGraphicsCommandList* const cmdList, const D3D12FrameInfo& d3d12FrameInfo, d3dx::D3D12ResourceBarrierList& barriers);
id_t AddLightCuller();
void RemoveLightCuller(id_t cullerID);

D3D12_GPU_VIRTUAL_ADDRESS GetLightGridOpaque(u32 lightCullingID, u32 frameIndex);
D3D12_GPU_VIRTUAL_ADDRESS GetLightIndexListOpaque(u32 lightCullingID, u32 frameIndex);
}