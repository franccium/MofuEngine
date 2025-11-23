#pragma once
#include "CommonHeaders.h"

namespace mofu::graphics::debug {
enum DebugMode : u32
{
	Default = 0,
	Depth,
	Normals,
	MaterialIDs,
	MotionVectors,

	Count
};
struct Settings
{
	//TODO: should have a bindless constant buffer with all settings relevant to shaders
	bool ReflectionsEnabled{ true };
	bool Reflections_FFXSSSR{ true };
	f32 ReflectionsStrength{ 1.f };
	bool ApplyDualKawaseBlur{ true };
	bool VB_HalfRes{ false };
	bool UsePrefilteredSpecular{ true };
	bool ApplyTonemap{ false };
	bool EnablePhysicsDebugRendering{ true };
	bool DrawPhysicsWorldBounds{ false };
	bool RenderAllPhysicsShapes{ false };

	struct FFX_SSSR_Settings
	{
		f32 DepthBufferThickness{ 0.015f };
		f32 RoughnessThreshold{ 0.4f };
		f32 IBLFactor{ 1.5f };
		f32 TemporalStabilityFactor{ 0.7f };
		f32 VarianceThreshold{ 0.0f };
		u32 MaxTraversalIntersections{ 24 };
		u32 MinTraversalOccupancy{ 4 };
		u32 MostDetailedMip{ 0 };
		u32 SamplesPerQuad{ 1 };
		bool TemporalVarianceGuidedTracingEnabled{ true };
		bool WasAmbientLightChanged{ true };
	} FFX_SSSR;

	struct SSILVB_Settings
	{
		f32 SampleCount{ 4.0f };
		f32 SampleRadius{ 4.0f };
		f32 SliceCount{ 4.0f };
		f32 HitThickness{ 0.5f };
	} SSILVB;
};
extern Settings RenderingSettings;

DebugMode GetDebugMode();
void SetDebugMode(DebugMode mode);
void ToggleDebugPostProcessing();
bool IsUsingDebugPostProcessing();

}