#include "RenderingDebug.h"
#include "D3D12/D3D12PostProcess.h"

namespace mofu::graphics::debug {
namespace {
DebugMode _debugMode{ DebugMode::Default };
bool _isUsingDebugPostProcessing{ false };

} // anonymous namespace
Settings RenderingSettings{};

DebugMode
GetDebugMode()
{
    return _debugMode;
}

void
SetDebugMode(DebugMode mode)
{
    _debugMode = mode;
}

void 
ToggleDebugPostProcessing()
{
    _isUsingDebugPostProcessing = !_isUsingDebugPostProcessing;
    d3d12::fx::SetDebug(_isUsingDebugPostProcessing);
}

bool 
IsUsingDebugPostProcessing()
{
    return _isUsingDebugPostProcessing;
}

}