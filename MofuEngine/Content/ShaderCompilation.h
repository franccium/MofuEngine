#pragma once
#include "CommonHeaders.h"
#include "Shaders/ShaderData.h"

namespace mofu::shaders {
std::unique_ptr<u8[]> CompileShader(ShaderFileInfo info, u8* code, 
	u32 codeSize, mofu::Vec<std::wstring>& extraArgs, bool debug = false, bool includeErrorsAndDisassembly = false);
std::unique_ptr<u8[]> CompileShader(ShaderFileInfo info, const char* path,
	mofu::Vec<std::wstring>& extraArgs, bool debug = false, bool includeErrorsAndDisassembly = false);
bool CompileEngineShaders();
bool CompileContentProcessingShaders();

void UpdateHotReload();
}