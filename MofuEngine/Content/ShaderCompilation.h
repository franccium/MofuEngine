#pragma once
#include "CommonHeaders.h"
#include "Graphics/Renderer.h"

struct ShaderFileInfo
{
	const char* file;
	const char* entryPoint;
	mofu::graphics::ShaderType::type type;
};

std::unique_ptr<u8[]> CompileShader(ShaderFileInfo info, u8* code, 
	u32 codeSize, mofu::Vec<std::wstring>& extraArgs, bool includeErrorsAndDisassembly = false);
std::unique_ptr<u8[]> CompileShader(ShaderFileInfo info, const char* path,
	mofu::Vec<std::wstring>& extraArgs, bool includeErrorsAndDisassembly = false);
bool CompileEngineShaders();