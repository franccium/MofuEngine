#pragma once
#include "D3D12CommonHeaders.h"
#include "Content/EngineShaders.h"

namespace mofu::graphics::d3d12::shaders {

bool Initialize();
void Shutdown();
void ReloadShader(EngineShader::ID id);

D3D12_SHADER_BYTECODE GetEngineShader(EngineShader::ID id);
D3D12_SHADER_BYTECODE GetDebugEngineShader(EngineDebugShader::ID id);
}