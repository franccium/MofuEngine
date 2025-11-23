#include "RenderingConsole.h"
#include "Input/InputSystem.h"
#include "Graphics/RenderingDebug.h"
#include "Graphics/RTSettings.h"
#include "ValueDisplay.h"
#include "Content/ContentUtils.h"
#include "Project/Project.h"
#include "Content/EditorContentManager.h"
#include "Utilities/Logger.h"

namespace mofu::editor::debug {
namespace {
bool _isOpen{ true };

u32 _currentSampleIndex{ 0 };
u64 _vertexCount{ 0 };
u64 _indexCount{ 0 };
u64 _lastAccelerationStructureBuildFrame{ 0 };
Vec<std::string> _skyFiles{};


void
DrawPostProcessingOptions()
{
	


}

void
DrawRayTracingInfo()
{
	ImGui::TextUnformatted("Path Trace Info:");
	ImGui::Text(" Current Sample: %u/%u", _currentSampleIndex, graphics::rt::settings::PPSampleCount);
	ImGui::TextUnformatted("Acceleration Structure:");
	ImGui::Text(" Last Built: %llu", _lastAccelerationStructureBuildFrame);
	ImGui::Text(" Vertex Count: %llu", _vertexCount);
	ImGui::Text(" Index Count: %llu", _indexCount);
}

void 
RefreshCubemapFiles()
{
	//TODO: placeholder
	_skyFiles.clear();
	Vec<std::string> cubemapFiles{};
	content::ListFilesByExtensionRec(".tex", project::GetResourceDirectory() / "Cubemaps", cubemapFiles);
	for (const auto& file : cubemapFiles)
	{
		if (file.ends_with("_sky.tex"))
		{
			_skyFiles.emplace_back(file);
		}
	}
}

void
DrawRayTracingSettings()
{
	using namespace graphics::rt::settings;
	Settings& settings{ RTGlobalSettings };
	ImGui::Checkbox("Enable Path Tracing", &PathTracingEnabled);
	ImGui::Checkbox("Always Restart Tracing", &AlwaysRestartPathTracing);
	ImGui::Checkbox("Always New Sample", &AlwaysNewSample);

	ImGui::Checkbox("Sun From DirLight", (bool*)&settings.SunFromDirectionalLight);
	ImGui::Checkbox("Indirect Enabled", (bool*)&settings.IndirectEnabled);
	ImGui::Checkbox("Specular Enabled", (bool*)&settings.SpecularEnabled);
	ImGui::Checkbox("Diffuse Enabled", (bool*)&settings.DiffuseEnabled);
	ImGui::Checkbox("Show Normals", (bool*)&settings.ShowNormals);
	ImGui::Checkbox("Show Ray Directions", (bool*)&settings.ShowRayDirs);
	ImGui::Checkbox("Render Skybox", (bool*)&settings.RenderSkybox);
	ImGui::Checkbox("Enabled Sun", (bool*)&settings.SunEnabled);
	ui::DisplayColorPicker((v3*)&SunIrradiance, "Sun Irradiance");

	ImGui::Checkbox("Shadows Only", (bool*)&settings.ShadowsOnly);

	ui::DisplayEditableFloatNT(&settings.DiffuseSpecularSelector, "Diffuse/Specular Selector", 0.f, 1.f);

	constexpr const char* BRDF_OPTIONS[BRDF_COUNT]{ "0" };
	ImGui::TextUnformatted("BRDF Type: ", BRDF_OPTIONS[settings.BRDFType]);
	for (u32 i{ 0 }; i < BRDF_COUNT; ++i)
	{
		if (ImGui::Button(BRDF_OPTIONS[i]))
		{
			settings.BRDFType = i;
		}
		ImGui::SameLine();
	}
	ImGui::NewLine();
	ImGui::Checkbox("Apply Energy Conservation", (bool*)&settings.ApplyEnergyConservation);
	ImGui::Checkbox("Use Russian Roulette", (bool*)&settings.UseRussianRoulette);

	ui::DisplayEditableUintNT(&settings.PPSampleCountSqrt, "Sample Count (Sqrt)", 1, 16);
	PPSampleCount = settings.PPSampleCountSqrt * settings.PPSampleCountSqrt;
	ui::DisplayEditableUintNT(&settings.MaxPathLength, "Max Path Length", 1, 6);
	ui::DisplayEditableUintNT(&settings.MaxAnyHitPathLength, "Max Any-Hit Path Length", 0, 6);
}

} // anonymous namespace

void 
InitializeRenderingConsole()
{
	RefreshCubemapFiles();
}

void
DrawRenderingConsole()
{
	graphics::debug::Settings& settings{ graphics::debug::RenderingSettings };
#if USE_DEBUG_KEYBINDS
	if (input::WasKeyPressed(input::Keybinds::Editor.ToggleRenderingConsole))
	{
		_isOpen = !_isOpen;
	}

	if (input::WasKeyPressed(input::Keybinds::Editor.TogglePhysicsDebugRendering))
	{
		settings.EnablePhysicsDebugRendering = !settings.EnablePhysicsDebugRendering;
	}
#endif

	if (!ImGui::Begin("Rendering", &_isOpen))
	{
		ImGui::End();
		return;
	}

	ImGui::BeginGroup();
	ImGui::Checkbox("Apply Tonemap", &settings.ApplyTonemap);
	ImGui::Checkbox("Enable Physics Debug Render", &settings.EnablePhysicsDebugRendering);
	ImGui::Checkbox("Draw Physics World Bounds", &settings.DrawPhysicsWorldBounds);
	ImGui::Checkbox("Render All Physics Shapes", &settings.RenderAllPhysicsShapes);
	ImGui::EndGroup();

	if (ImGui::CollapsingHeader("Reflections", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Enable Reflections", &settings.ReflectionsEnabled);
		ui::DisplayEditableFloatNT(&settings.ReflectionsStrength, "Reflections Strength", 0.f, 8.f);
		ImGui::Checkbox("VB at half res", &settings.VB_HalfRes);
		ImGui::Checkbox("Apply Dual Kawase Blur", &settings.ApplyDualKawaseBlur);
		ImGui::Checkbox("Use Prefiltered Specular", &settings.UsePrefilteredSpecular);

		if (ImGui::CollapsingHeader("FFX SSSR", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ui::DisplayEditableFloatNT(&settings.FFX_SSSR.DepthBufferThickness, "Depth Buffer Thickness", 0.f, 0.2f);
			ui::DisplayEditableFloatNT(&settings.FFX_SSSR.RoughnessThreshold, "Roughness Threshold", 0.f, 1.f);
			ui::DisplayEditableFloatNT(&settings.FFX_SSSR.IBLFactor, "IBL Factor", 0.f, 5.f);
			ui::DisplayEditableFloatNT(&settings.FFX_SSSR.TemporalStabilityFactor, "Temporal Stability Factor", 0.f, 1.f);
			ui::DisplayEditableFloatNT(&settings.FFX_SSSR.VarianceThreshold, "Variance Threshold", 0.f, 1.f);
			ui::DisplayEditableUintNT(&settings.FFX_SSSR.MaxTraversalIntersections, "Max Traversal Intersections", 1, 150);
			ui::DisplayEditableUintNT(&settings.FFX_SSSR.MinTraversalOccupancy, "Min Traversal Occupancy", 0, 100);
			ui::DisplayEditableUintNT(&settings.FFX_SSSR.MostDetailedMip, "Most Detailed Mip", 0, 13);
			ui::DisplayEditableUintNT(&settings.FFX_SSSR.SamplesPerQuad, "Samples Per Quad", 1, 8);
			ImGui::Checkbox("Tmp Variance Guided Tracing", &settings.FFX_SSSR.TemporalVarianceGuidedTracingEnabled);
		}

		if (ImGui::CollapsingHeader("SSILVB", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ui::DisplayEditableFloatNT(&settings.SSILVB.SampleCount, "SampleCount", 1.f, 8.f);
			ui::DisplayEditableFloatNT(&settings.SSILVB.SampleRadius, "Sample Radius", 1.f, 6.f);
			ui::DisplayEditableFloatNT(&settings.SSILVB.SliceCount, "Slice Count", 1.f, 8.f);
			ui::DisplayEditableFloatNT(&settings.SSILVB.HitThickness, "Hit Thickness", 0.f, 1.f);
		}
	}

	bool isInDebugPostProcessing{ graphics::debug::IsUsingDebugPostProcessing() };
	u32 debugPostProcessingOption{ isInDebugPostProcessing ? 1u : 0u };
	constexpr const char* USE_DEBUG_TEXT[]{ "Use Debug Post Processing", "Turn Off Debug Post Processing" };
	if (ImGui::Button(USE_DEBUG_TEXT[debugPostProcessingOption]))
	{
		graphics::debug::ToggleDebugPostProcessing();
		isInDebugPostProcessing = !isInDebugPostProcessing;
	}

#if RAYTRACING
	if (ImGui::CollapsingHeader("Path Tracing"), ImGuiTreeNodeFlags_DefaultOpen)
	{
		DrawRayTracingSettings();
		DrawRayTracingInfo();
	}
#endif

	if (ImGui::CollapsingHeader("Post Processing"))
	{
		DrawPostProcessingOptions();

		if (ImGui::TreeNode("Tone Mapping"))
		{
			ImGui::TreePop();
		}
	}

	if (ImGui::CollapsingHeader("Debug Rendering", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::BeginDisabled(!isInDebugPostProcessing);
		constexpr const char* DEBUG_MODES[]{ "Default", "Depth", "Normals", "Material IDs", "Motion Vectors"};
		u32 chosenMode{ graphics::debug::GetDebugMode() };
		ImGui::TextUnformatted("Mode: ", DEBUG_MODES[chosenMode]);
		for (u32 i{ 0 }; i < _countof(DEBUG_MODES); ++i)
		{
			if (ImGui::Button(DEBUG_MODES[i]))
			{
				graphics::debug::SetDebugMode((graphics::debug::DebugMode)i);
			}
			ImGui::SameLine();
		}
		ImGui::EndDisabled();
	}

	if (ImGui::CollapsingHeader("Ambient Light", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ui::DisplayEditableFloatNT(graphics::light::GetAmbientIntensityRef(), "Intensity", 0.f, 5.f);
		ui::DisplayEditableUintNT(graphics::light::GetCurrentLightSetKeyRef(), "Light Set", 0.f, 5.f);

		//TODO: very placeholder code
		if(ImGui::Button("Refresh Cubemaps"))
		{
			RefreshCubemapFiles();
		}
		if (ImGui::Button("Change Skybox"))
		{
			ImGui::OpenPopup("SelectTexturePopup");
		}
		if (ImGui::BeginPopup("SelectTexturePopup"))
		{
			for (std::string_view path : _skyFiles)
			{
				if (ImGui::Selectable(path.data()))
				{
					content::AssetHandle skyHandle{ content::assets::GetHandleFromImportedPath(path) };
					if (skyHandle != content::INVALID_HANDLE)
					{
						content::AssetPtr skybox{ content::assets::GetAsset(skyHandle) };
						content::assets::AmbientLightHandles handles{ content::assets::GetAmbientLightHandles(skyHandle) };
						content::AssetHandle diffuseHandle{ content::assets::GetIBLRelatedHandle(skyHandle) };
						content::AssetHandle specularHandle{ content::assets::GetIBLRelatedHandle(diffuseHandle) };
						content::AssetHandle brdfLUTHandle{ content::assets::GetIBLRelatedHandle(specularHandle) };

						f32 ambientLightIntensity{ 1.f };
						id_t diffuseIBL{ content::assets::CreateResourceFromHandle(handles.DiffuseHandle) };
						id_t specularIBL{ content::assets::CreateResourceFromHandle(handles.SpecularHandle) };
						id_t BRDFLutIBL{ content::assets::CreateResourceFromHandle(handles.BrdfLutHandle) };
						id_t SkyboxHandle{ content::assets::CreateResourceFromHandle(skyHandle) };
						graphics::light::AddAmbientLight(graphics::light::GetCurrentLightSetKey(), 
							{ambientLightIntensity, diffuseIBL, specularIBL, BRDFLutIBL, SkyboxHandle});
						settings.FFX_SSSR.WasAmbientLightChanged = true;
					}
					else
					{
						log::Warn("Can't find assetHandle from path");
					}
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndPopup();
		}
	}
	
	ImGui::End();
}

void
UpdateAccelerationStructureData(u64 vertexCount, u64 indexCount, u64 lastBuildFrame)
{
	_vertexCount = vertexCount;
	_indexCount = indexCount;
	_lastAccelerationStructureBuildFrame = lastBuildFrame;
}

void
UpdatePathTraceSampleData(u32 currentSampleIndex)
{
	_currentSampleIndex = currentSampleIndex;
}

}