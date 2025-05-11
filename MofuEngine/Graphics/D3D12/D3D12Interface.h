#pragma once

namespace mofu::graphics {
struct PlatformInterface;

namespace d3d12 {
void SetupPlatformInterface(PlatformInterface& pi);

}

}