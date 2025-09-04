#include "RenderingConsole.h"
#include "Input/InputSystem.h"
#include "Graphics/RenderingDebug.h"

namespace mofu::editor::debug {
namespace {
bool _isOpen{ false };
bool _postProcessingOpen{ false };
bool _drawNormals{ false };

void
DrawPostProcessingOptions()
{
	


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

}