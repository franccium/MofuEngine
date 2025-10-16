#pragma once
#include "D3D12CommonHeaders.h"	

namespace mofu::graphics::d3d12 {
struct D3D12FrameInfo;
}

namespace mofu::graphics::d3d12::fx {
bool Initialize();
void Shutdown();

void SetDebug(bool debugOn);

void SetBufferSize(u32v2 size);
void AddTransitionsPrePostProcess(d3dx::D3D12ResourceBarrierList& barriers);
void AddTransitionsPostPostProcess(d3dx::D3D12ResourceBarrierList& barriers);
void DoPostProcessing(DXGraphicsCommandList* cmdList, const D3D12FrameInfo& frameInfo, D3D12_CPU_DESCRIPTOR_HANDLE rtv);
D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGPUDescriptorHandle();
void ResetShaders(bool debug);
}