#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::shaders {
struct EngineShader
{
	enum id : u32
	{
		FullscreenTriangleVS = 0,
		//ColorFillPS,
		PostProcessPS,
		CalculateGridFrustumsCS,
		LightCullingCS,
		RayTracingLib,
		Count
	};
};

struct EngineDebugShader
{
	enum id : u32
	{
		PostProcessPS,
		Count
	};
};

bool Initialize();
void Shutdown();

D3D12_SHADER_BYTECODE GetEngineShader(EngineShader::id id);
D3D12_SHADER_BYTECODE GetDebugEngineShader(EngineDebugShader::id id);
}