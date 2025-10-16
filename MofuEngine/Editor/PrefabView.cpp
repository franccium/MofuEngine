#include "PrefabView.h"
#include "ValueDisplay.h"
#include "imgui.h"
#include "SceneEditorView.h"

namespace mofu::editor::scene {
namespace {


} // anonymous namespace

void
InspectPrefab(const std::filesystem::path& path)
{
	ImGui::Text("Prefab: %ull", -1ull);

	if (ImGui::Button("Add To Scene")) LoadPrefab(path);
}

void
LoadPrefab(const std::filesystem::path& path)
{
	AddPrefab(path);
}

}
