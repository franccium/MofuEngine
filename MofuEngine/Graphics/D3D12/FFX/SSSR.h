#pragma once
#include "../D3D12CommonHeaders.h"
#include "FidelityFX/host/ffx_sssr.h"
#include "FidelityFX/host/backends/dx12/ffx_dx12.h"
#include "Graphics/D3D12/D3D12Core.h"

namespace mofu::graphics::d3d12::ffx::sssr {
constexpr DXGI_FORMAT SSSR_OUTPUT_FORMAT{ DXGI_FORMAT_R16G16B16A16_FLOAT };

void Initialize();
void Shutdown();
void Dispatch(const D3D12FrameInfo& frameInfo);
void GatherResources();

const D3D12RenderTexture& ReflectionsBuffer();
}