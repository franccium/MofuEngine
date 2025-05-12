#pragma once
#include "D3D12CommonHeaders.h"	

namespace mofu::graphics::d3d12 {
struct D3D12FrameInfo;
}

namespace mofu::graphics::d3d12::fx {
bool Initialize();
void Shutdown();

void DoPostProcessing(DXGraphicsCommandList* cmdList, const D3D12FrameInfo& frameInfo, D3D12_CPU_DESCRIPTOR_HANDLE targetRtv);
}