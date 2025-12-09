#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::transparency {
bool Initialize();
void Shutdown();
void CreateBuffers(u32v2 renderDimensions);
void AddBarriersForRendering(d3dx::D3D12ResourceBarrierList& barriers);
void AddBarriersForReading(d3dx::D3D12ResourceBarrierList& barriers);
D3D12_GPU_VIRTUAL_ADDRESS GetTransparencyHeadBufferGPUAddr();
D3D12_GPU_VIRTUAL_ADDRESS GetTransparencyListGPUAddr();
u32 GetTransparencyHeadBufferIndex();
u32 GetTransparencyListIndex();
void ClearBuffers(DXGraphicsCommandList* const cmdList);
}