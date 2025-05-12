#include "D3D12Shaders.h"

namespace mofu::graphics::d3d12::shaders {
namespace {

} // anonymous namespace

bool 
Initialize()
{
    return true;
}

void 
Shutdown()
{
}

D3D12_SHADER_BYTECODE 
GetEngineShader(EngineShader::id id)
{
    assert(id < EngineShader::Count);
    return {};
}

}
