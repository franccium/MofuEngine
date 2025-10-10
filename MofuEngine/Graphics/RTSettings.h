#pragma once
#include "CommonHeaders.h"

namespace mofu::graphics::rt::settings {
static constexpr u32 PP_SAMPLE_COUNT_SQRT{ 3 };
static constexpr u32 PP_SAMPLE_COUNT{ PP_SAMPLE_COUNT_SQRT * PP_SAMPLE_COUNT_SQRT };
static constexpr u32 MAX_TRACE_RECURSION_DEPTH{ 2 };
static constexpr bool ALWAYS_RESTART_TRACING{ true };
static constexpr bool ALWAYS_NEW_SAMPLE{ true };

extern v3 SunIrradiance;
extern v3 SunDirection;
extern v3 SunColor;
extern f32 SunAngularRadius;

void Initialize();

u32 GetSceneMeshCount();
}