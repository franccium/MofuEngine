#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12 {
class D3D12Surface;
}

namespace mofu::graphics::d3d12::ui {
bool Initialize(ID3D12CommandQueue* queue);
void Shutdown();

void StartNewFrame();

void SetupGUIFrame();
void RenderGUI();
void RenderSceneIntoImage(D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle, D3D12FrameInfo frameInfo);
void EndGUIFrame(DXGraphicsCommandList* cmdList);

void ViewTextureAsImage(id_t textureID);
void DestroyViewTexture(id_t textureID);
u64 GetImTextureID(id_t textureID, u32 mipLevel, u32 format);


}