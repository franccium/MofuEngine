#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::shaders {
struct EngineShader
{
	enum id : u32
	{
		FullScreenTriangleVS = 0,
		FullScreenTrianglePS,
		PostProcessPS,
		Count
	};
};

bool Initialize();
void Shutdown();

D3D12_SHADER_BYTECODE GetEngineShader(EngineShader::id id);
}