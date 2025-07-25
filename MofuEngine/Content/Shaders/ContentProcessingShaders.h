#pragma once
#include "CommonHeaders.h"
#include "Content/EngineShaders.h"

namespace mofu::content::shaders {
struct ShaderBytecode
{
	const void* Bytecode;
	u64 BytecodeLength;
};

bool Initialize();
void Shutdown();
ShaderBytecode GetContentShader(ContentShader::ID id);
}