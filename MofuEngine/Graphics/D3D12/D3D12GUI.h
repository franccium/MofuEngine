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
// free a texture resource used in editor
void DestroyViewTexture(id_t textureID);
[[nodiscard]] u64 GetImTextureIDIcon(id_t textureID, u32 mipLevel, u32 format);
[[nodiscard]] u64 GetImTextureID(id_t textureID, u32 arrayIndex, u32 mipLevel, u32 depthIndex, u32 format, bool isCubemap = false);


}