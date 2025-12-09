#pragma once
#include "../D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::particles {
bool Initialize();
void Shutdown();
void Update(DXGraphicsCommandList* const cmdList, const D3D12FrameInfo& frameInfo);
void Render();
void AddBarriersForSimulation(d3dx::D3D12ResourceBarrierList& barriers);
void AddBarriersForRendering(d3dx::D3D12ResourceBarrierList& barriers);
void ResetShaders();
}