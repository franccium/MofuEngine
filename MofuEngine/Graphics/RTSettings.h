#pragma once
#include "CommonHeaders.h"

namespace mofu::graphics::rt::settings {
static constexpr u32 MAX_TRACE_RECURSION_DEPTH{ 3 };
static constexpr u32 BRDF_COUNT{ 1 };
static_assert(MAX_TRACE_RECURSION_DEPTH > 0 && MAX_TRACE_RECURSION_DEPTH < 5);
struct Settings
{
	u32 PPSampleCountSqrt{ 9 };
	u32 MaxPathLength{ 3 };
	u32 MaxAnyHitPathLength{ 1 };

	u32 IndirectEnabled{ false };
	u32 SpecularEnabled{ true };
	u32 DiffuseEnabled{ true };
	u32 SunFromDirectionalLight{ true };
	u32 RenderSkybox{ true };
	u32 ShowNormals{ false };
	u32 ShowRayDirs{ false };
	u32 SunEnabled{ true };
	u32 ShadowsOnly{ false };
	f32 DiffuseSpecularSelector{ 0.5f };

	u32 BRDFType{ 0 };
	u32 ApplyEnergyConservation{ true };
	u32 UseRussianRoulette{ true };
};
extern Settings RTGlobalSettings;

extern bool PathTracingEnabled;
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