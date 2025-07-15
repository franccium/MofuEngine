#include "ImporterView.h"
#include "imgui.h"
#include "MaterialEditor.h"
#include "AssetInteraction.h"

namespace mofu::editor {
namespace {

bool isOpen{ false }; // TODO: something better this is placeholder
content::FBXImportState fbxState;

} // anonymous namespace

void
ViewFBXImportSummary(content::FBXImportState state)
{
	isOpen = true;
	fbxState = state;
}

constexpr const char* ErrorString[7]{
	"None",
	"InvalidTexturePath",
	"MaterialTextureNotFound",
	"Test1",
	"Test2",
	"Test3",
	"Test4",
};

void
RenderImportSummary()
{
	if (!isOpen) return;

	ImGui::Begin("FBX Import Summary", nullptr);

	ImGui::Text("File: %s", fbxState.FbxFile);
	if (fbxState.Errors)
	{
		ImGui::TextColored({ 0.8f, 0.1f, 0.1f, 1.f }, "Errors:");
		for (u32 i{ 1 }; i < 32; ++i)
		{
			if (fbxState.Errors & (1u << i))
			{
				ImGui::TextColored({ 0.8f, 0.1f, 0.1f, 1.f }, ErrorString[i]);
			}
		}
	}
	
	ImGui::Text("Image Files: [%u]", fbxState.ImageFiles.size());
	for (std::string_view file : fbxState.ImageFiles)
	{
		ImGui::TextUnformatted(file.data());
	}

	ImGui::Text("Materials: [%u]", fbxState.Materials.size());
	for (editor::material::EditorMaterial& mat : fbxState.Materials)
	{
		//TODO: display vec4s etc
		ImGui::TextUnformatted(mat.Name.data());
		ImGui::Text("Type: %u", mat.Type);
		editor::material::DisplayMaterialSurfaceProperties(mat.Surface);
	}

	if (ImGui::Button("Add To Scene"))
	{
		assets::AddFBXImportedModelToScene(fbxState);
	}

	ImGui::End();
}

}
