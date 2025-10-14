#include "RenderingConsole.h"
#include "Input/InputSystem.h"
#include "Graphics/RenderingDebug.h"
#include "Graphics/RTSettings.h"
#include "ValueDisplay.h"

namespace mofu::editor::debug {
namespace {
bool _isOpen{ true };

u32 _currentSampleIndex{ 0 };
u64 _vertexCount{ 0 };
u64 _indexCount{ 0 };
u64 _lastAccelerationStructureBuildFrame{ 0 };

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
DrawRayTracingSettings()
{
	using namespace graphics::rt::settings;
	Settings& settings{ RTGlobalSettings };
	ImGui::Checkbox("Always Restart Tracing", &AlwaysRestartPathTracing);
	ImGui::Checkbox("Always New Sample", &AlwaysNewSample);

	ImGui::Checkbox("Sun From DirLight", (bool*)&settings.SunFromDirectionalLight);
	ImGui::Checkbox("Indirect Enabled", (bool*)&settings.IndirectEnabled);
	ImGui::Checkbox("Show Normals", (bool*)&settings.ShowNormals);
	ImGui::Checkbox("Show Ray Directions", (bool*)&settings.ShowRayDirs);
	ImGui::Checkbox("Render Skybox", (bool*)&settings.RenderSkybox);
	ImGui::Checkbox("Enabled Sun", (bool*)&settings.SunEnabled);
	ImGui::Checkbox("Shadows Only", (bool*)&settings.ShadowsOnly);

	DisplayEditableUintNT(&settings.PPSampleCountSqrt, "Sample Count (Sqrt)", 1, 16);
	PPSampleCount = settings.PPSampleCountSqrt * settings.PPSampleCountSqrt;
	DisplayEditableUintNT(&settings.MaxPathLength, "Max Path Length", 1, 6);
	DisplayEditableUintNT(&settings.MaxAnyHitPathLength, "Max Any-Hit Path Length", 0, 6);
}

} // anonymous namespace

void
DrawRenderingConsole()
{
#if USE_DEBUG_KEYBINDS
	if (input::WasKeyPressed(input::Keys::Key::K))
	{
		_isOpen = !_isOpen;
	}
#endif

	if (!ImGui::Begin("Rendering", &_isOpen))
	{
		ImGui::End();
		return;
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
		constexpr const char* DEBUG_MODES[]{ "Default", "Depth", "Normals", "Material IDs" };
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