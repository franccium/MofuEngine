#pragma once
#include "CommonHeaders.h"

namespace mofu::graphics::debug {
enum DebugMode : u32
{
	Default = 0,
	Depth,
	Normals,
	MaterialIDs,

	Count
};
struct Settings
{
	bool EnablePhysicsDebugRendering{ true };
	bool DrawPhysicsWorldBounds{ false };
	bool RenderAllPhysicsShapes{ false };
};
extern Settings RenderingSettings;

DebugMode GetDebugMode();
void SetDebugMode(DebugMode mode);
void ToggleDebugPostProcessing();
bool IsUsingDebugPostProcessing();

}