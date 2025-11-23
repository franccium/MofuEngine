#pragma once
#include "Graphics/D3D12/D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::effects {
void ApplyKawaseBlur(DXResource* res, u32 texSrvIndex, DXGraphicsCommandList* const cmdList);
void Initialize();
void Shutdown();
void CreatePSOs();
u32 GetBlurUpResultSrvIndex();

const D3D12_VIEWPORT* GetLowResViewport();
const D3D12_RECT* GetLowResScissorRect();
}