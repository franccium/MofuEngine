#pragma once
#include "CommonHeaders.h"

namespace mofu::graphics::rt::settings {
static constexpr u32 MAX_TRACE_RECURSION_DEPTH{ 3 };
static_assert(MAX_TRACE_RECURSION_DEPTH > 0 && MAX_TRACE_RECURSION_DEPTH < 5);
struct Settings
{
	u32 PPSampleCountSqrt{ 9 };
	u32 MaxPathLength{ 3 };
	u32 MaxAnyHitPathLength{ 1 };
	bool IndirectEnabled{ false };
	bool SunFromDirectionalLight{ true };
	bool RenderSkybox{ false };
	bool ShowNormals{ true };
	bool ShowRayDirs{ false };
};
extern Settings RTGlobalSettings;

extern u32 PPSampleCount;
extern bool AlwaysRestartPathTracing;
extern bool AlwaysNewSample;

extern v3 SunIrradiance;
extern v3 SunDirection;
extern v3 SunColor;
extern f32 SunAngularRadius;

void Initialize();

u32 GetSceneMeshCount();
}