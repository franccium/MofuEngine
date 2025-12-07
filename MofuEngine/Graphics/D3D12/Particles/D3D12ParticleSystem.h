#pragma once
#include "../D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::particles {
bool Initialize();
void Shutdown();
void Update();
}