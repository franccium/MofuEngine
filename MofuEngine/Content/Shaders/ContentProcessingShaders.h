#pragma once
#include "CommonHeaders.h"
#include "Content/EngineShaders.h"
#include "Graphics/D3D12/D3D12CommonHeaders.h"

namespace mofu::shaders::content {
struct ShaderBytecode
{
	const void* Bytecode;
	u64 BytecodeLength;
};

bool Initialize();
void Shutdown();
//ShaderBytecode GetContentShader(ContentShader::ID id);
D3D12_SHADER_BYTECODE GetContentShader(ContentShader::ID id);
}