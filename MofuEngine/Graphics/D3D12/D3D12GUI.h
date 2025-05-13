#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12 {
class D3D12Surface;
}

namespace mofu::graphics::d3d12::gui {
bool Initialize(DXGraphicsCommandList* cmdList, ID3D12CommandQueue* queue);
void SetupGUIFrame();
void RenderGUI(DXGraphicsCommandList* cmdList);
void RenderTextureIntoImage(DXGraphicsCommandList* cmdList, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle, D3D12FrameInfo frameInfo);
void EndGUIFrame(DXGraphicsCommandList* cmdList);
}