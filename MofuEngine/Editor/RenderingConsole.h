#pragma once
#include "imgui.h"
#include "EditorStyle.h"

#define USE_DEBUG_KEYBINDS 1

namespace mofu::editor::debug {
void InitializeRenderingConsole();
void DrawRenderingConsole();

void UpdateAccelerationStructureData(u64 vertexCount, u64 indexCount, u64 lastBuildFrame);
void UpdatePathTraceSampleData(u32 currentSampleIndex);
}