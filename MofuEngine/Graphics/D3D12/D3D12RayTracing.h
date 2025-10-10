#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::rt {
bool CompileRTShaders();
bool InitializeRT();
void Shutdown();
bool CreateRootSignature();
bool CreatePSO();
bool CreateBuffers(u32v2 size);
void Update(bool shouldRestartPathTracing, bool renderItemsUpdated);
void Render(const D3D12FrameInfo& frameInfo, DXGraphicsCommandList* const cmdList);
DescriptorHandle MainBufferSRV();
void RequestRTUpdate();
void RequestRTAccStructureRebuild();
void UpdateAccelerationStructure();
}