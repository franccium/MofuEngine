#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12 {
	struct D3D12FrameInfo;
}

namespace mofu::graphics::d3d12::vbao {
bool Initialize();
void Shutdown();
}