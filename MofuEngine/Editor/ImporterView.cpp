#include "ImporterView.h"
#include "imgui.h"
#include "MaterialEditor.h"
#include "AssetInteraction.h"
#include "Content/TextureImport.h"
#include "TextureView.h"
#include "Content/EditorContentManager.h"

namespace mofu::editor::assets {
void RenderTextureImportSettings();

namespace {

bool isOpen{ false }; // TODO: something better this is placeholder
content::FBXImportState fbxState;
content::AssetType::type activeAssetImportType{ content::AssetType::Unknown };

content::GeometryImportSettings geometryImportSettings{};
content::texture::TextureImportSettings textureImportSettings{};

void
RenderUnknownSettings()
{
	ImGui::TextUnformatted("Import Settings...");
}

void
RenderMeshImportSettings()
{
	ImGui::TextUnformatted("Mesh Import Settings");
	ImGui::Checkbox("Import Embedded Textures", &geometryImportSettings.ImportEmbeddedTextures);
	ImGui::Checkbox("Calculate Tangents", &geometryImportSettings.CalculateTangents);
	ImGui::Checkbox("Calculate Normals", &geometryImportSettings.CalculateNormals);
	ImGui::Checkbox("Import Animations", &geometryImportSettings.ImportAnimations);
	ImGui::Checkbox("Merge Meshes", &geometryImportSettings.MergeMeshes);
	ImGui::Checkbox("Reverse Handedness", &geometryImportSettings.ReverseHandedness);
	ImGui::DragFloat("Smoothing Angle", &geometryImportSettings.SmoothingAngle, 0.5f, 0.f, 180.f);

	if (ImGui::Button("Restore Defaults")) geometryImportSettings = {};
}


using SettingsRenderer = void(*)();
constexpr std::array<SettingsRenderer, content::AssetType::Count> settingsRenderers {
	RenderUnknownSettings,
	RenderMeshImportSettings,
	RenderTextureImportSettings,
};

} // anonymous namespace

void
RenderTextureImportSettings()
{
	ImGui::TextUnformatted("Texture Import Settings");
	ImGui::TextUnformatted("Files:");
	for (std::string_view s : content::SplitString(textureImportSettings.Files, ';'))
	{
		ImGui::TextUnformatted(s.data());
	}
	if (ImGui::BeginCombo("Dimension", texture::DIMENSION_TO_STRING[textureImportSettings.Dimension]))
	{
		for (u32 i{ 0 }; i < content::texture::TextureDimension::Count; ++i)
		{
			const char* dimStr{ texture::DIMENSION_TO_STRING[i] };
			bool chosen{ i == textureImportSettings.Dimension };
			if (ImGui::Selectable(dimStr, chosen))
				textureImportSettings.Dimension = (content::texture::TextureDimension::Dimension)i;
			if (chosen)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	DisplaySliderUint("Mip Levels", &textureImportSettings.MipLevels, 0, 14);
	ImGui::SliderFloat("Alpha Threshold", &textureImportSettings.AlphaThreshold, 0.f, 1.f);
	ImGui::TextUnformatted("Format: "); ImGui::SameLine();
	ImGui::TextUnformatted(texture::FORMAT_STRING);
	DisplaySliderUint("Cubemap Size", &textureImportSettings.CubemapSize, 16, 4096);

	ImGui::Checkbox("Prefer BC7", (bool*)&textureImportSettings.PreferBC7);
	ImGui::Checkbox("Compress", (bool*)&textureImportSettings.Compress);
	ImGui::Checkbox("Prefilter Cubemap", (bool*)&textureImportSettings.PrefilterCubemap);
	ImGui::Checkbox("Mirror Cubemap", (bool*)&textureImportSettings.MirrorCubemap);

	if (ImGui::Button("Restore Defaults")) textureImportSettings = {};
}

void
ViewFBXImportSummary(content::FBXImportState state)
{
	isOpen = true;
	fbxState = state;
}

void 
ViewImportSettings(content::AssetType::type assetType)
{
	activeAssetImportType = assetType;
}

void
ViewImportSettings(content::AssetHandle handle)
{
	content::AssetPtr asset{ content::assets::GetAsset(handle) };
	activeAssetImportType = asset->Type;
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

	ImGui::Text("File: %s", fbxState.FbxFile.data());
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

void 
RenderImportSettings()
{
	ImGui::Begin("Import settings", nullptr);

	settingsRenderers[activeAssetImportType]();

	ImGui::End();
}

content::GeometryImportSettings GetGeometryImportSettings()
{
	return geometryImportSettings;
}

content::texture::TextureImportSettings GetTextureImportSettings()
{
	return textureImportSettings;
}

}
