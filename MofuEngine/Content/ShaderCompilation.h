#pragma once
#include "CommonHeaders.h"
#include "Graphics/Renderer.h"

namespace mofu::shaders {
struct ShaderFileInfo
{
	const char* File;
	const char* EntryPoint;
	mofu::graphics::ShaderType::Type Type;
};

std::unique_ptr<u8[]> CompileShader(ShaderFileInfo info, u8* code, 
	u32 codeSize, mofu::Vec<std::wstring>& extraArgs, bool debug = false, bool includeErrorsAndDisassembly = false);
std::unique_ptr<u8[]> CompileShader(ShaderFileInfo info, const char* path,
	mofu::Vec<std::wstring>& extraArgs, bool debug = false, bool includeErrorsAndDisassembly = false);
bool CompileEngineShaders();
bool CompileContentProcessingShaders();
}