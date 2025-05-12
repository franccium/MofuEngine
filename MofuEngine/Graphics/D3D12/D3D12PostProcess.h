#pragma once

namespace mofu::graphics::d3d12 {
struct d3d12_frame_info;
}

namespace mofu::graphics::d3d12::fx {
bool Initialize();
void Shutdown();
}