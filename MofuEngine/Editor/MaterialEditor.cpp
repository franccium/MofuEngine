#include "MaterialEditor.h"
#include "Content/ContentManagement.h"
#include "imgui.h"
#include "Graphics/Renderer.h"
#include "Graphics/UIRenderer.h"
#include "TextureView.h"
#include "ValueDisplay.h"
#include "EngineAPI/ECS/SceneAPI.h"

#include "Graphics/D3D12/D3D12Core.h"
#include "Project/Project.h"

namespace mofu::editor::material {
namespace {

constexpr const char* WHITE_TEXTURE{ "Projects/TestProject/Assets/EngineTextures/white_placeholder_texture.tex" };
constexpr const char* GRAY_TEXTURE{ "Projects/TestProject/Assets/EngineTextures/gray_placeholder_texture.tex" };
constexpr const char* BLACK_TEXTURE{ "Projects/TestProject/Assets/EngineTextures/black_placeholder_texture.tex" };
constexpr const char* ERROR_TEXTURE{ "Projects/TestProject/Assets/EngineTextures/error_texture.tex" };
constexpr const char* DEFAULT_TEXTURES_PATHS[TextureUsage::Count]{
	ERROR_TEXTURE,
	GRAY_TEXTURE,
	GRAY_TEXTURE,
	BLACK_TEXTURE,
	WHITE_TEXTURE
};
constexpr const char* MATERIAL_SURFACE_TYPE_TO_STRING[graphics::MaterialType::Count]{
	"Opaque",
};

id_t DEFAULT_TEXTURES[TextureUsage::Count];
id_t DEFAULT_SHADERS[graphics::ShaderType::Count]{ id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID };
id_t usedTextures[TextureUsage::Count];

ecs::Entity materialOwner{};
ecs::component::RenderMaterial material{};
EditorMaterial editorMaterial{};
graphics::MaterialInitInfo materialInitInfo{};

bool isOpen{ false };
bool isBrowserOpen{ false };
TextureUsage::Usage textureBeingChanged{};

Vec<std::string> texFiles{};

[[nodiscard]] id_t
LoadTexture(const char* path)
{
	return content::CreateResourceFromAsset(path, content::AssetType::Texture);
}

void
DisplayTexture(TextureUsage::Usage texUse, const char* label, const char* id)
{
	constexpr ImVec2 imageSize{ 64,64 };

	ImGui::Text(label); ImGui::SameLine();
	if (ImGui::ImageButton(id, graphics::ui::GetImTextureID(materialInitInfo.TextureIDs[texUse]), imageSize))
	{
		if (isBrowserOpen)
		{
			ImGui::CloseCurrentPopup();
		}
		texFiles.clear();
		content::ListFilesByExtension(".tex", project::GetResourcePath(), texFiles);
		ImGui::OpenPopup("Select Texture");
		isBrowserOpen = true;
		textureBeingChanged = texUse;
	}

	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		texture::OpenTextureView(materialInitInfo.TextureIDs[texUse]);
	}
}

void
RenderTextureBrowser()
{
	if (ImGui::BeginPopup("Select Texture"))
	{
		for (std::string_view texPath : texFiles)
		{
			if (ImGui::Selectable(texPath.data()))
			{
				materialInitInfo.TextureIDs[textureBeingChanged] = content::CreateResourceFromAsset(texPath, content::AssetType::Texture);
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}
}

} // anonymous namespace

bool 
InitializeMaterialEditor()
{
	for (u32 i{ 0 }; i < TextureUsage::Count; ++i)
	{
		DEFAULT_TEXTURES[i] = LoadTexture(DEFAULT_TEXTURES_PATHS[i]);
		assert(id::IsValid(DEFAULT_TEXTURES[i]));
	}
	return true;
}

void
DisplayMaterialSurfaceProperties(const graphics::MaterialSurface& surface)
{
	ImGui::TextUnformatted("Surface:");
	DisplayVector4(surface.BaseColor, "Base Color");
	DisplayFloat(surface.Metallic, "Metallic");
	DisplayFloat(surface.Roughness, "Roughness");
	DisplayVector3(surface.Emissive, "Emission Color");
	DisplayFloat(surface.EmissiveIntensity, "Emission Intensity");
	DisplayFloat(surface.AmbientOcclusion, "Ambient Occlusion");
}


void
OpenMaterialEditor(ecs::Entity entityID, ecs::component::RenderMaterial mat)
{
	materialOwner = entityID;
	material = mat;
	materialInitInfo = graphics::GetMaterialReflection(mat.MaterialIDs[0]);
	if (materialInitInfo.TextureCount < TextureUsage::Count)
	{
		//TODO: all sorts of wrong
		id_t* textures{ materialInitInfo.TextureIDs };
		materialInitInfo.TextureIDs = new id_t[TextureUsage::Count];
		memcpy(materialInitInfo.TextureIDs, textures, sizeof(id_t) * materialInitInfo.TextureCount);
		for (u32 i{ materialInitInfo.TextureCount }; i < TextureUsage::Count; ++i) materialInitInfo.TextureIDs[i] = id::INVALID_ID;
		materialInitInfo.TextureCount = TextureUsage::Count;
		delete[] textures;
	}
	for (u32 i{ 0 }; i < TextureUsage::Count; ++i)
	{
		if (!id::IsValid(materialInitInfo.TextureIDs[i]))
		{
			materialInitInfo.TextureIDs[i] = DEFAULT_TEXTURES[i];
		}
	}
	editorMaterial.Surface = materialInitInfo.Surface;
	isOpen = true;
}

void
OpenMaterialCreator(ecs::Entity entityID)
{
	materialOwner = entityID;
	material = ecs::scene::GetComponent<ecs::component::RenderMaterial>(entityID);
	materialInitInfo.Type = graphics::MaterialType::Opaque;
	materialInitInfo.Surface = graphics::MaterialSurface{};
	materialInitInfo.TextureCount = TextureUsage::Count;
	materialInitInfo.TextureIDs = &DEFAULT_TEXTURES[0];
	memcpy(materialInitInfo.ShaderIDs, DEFAULT_SHADERS, graphics::ShaderType::Count * sizeof(id_t));
	isOpen = true;
}

id_t 
GetDefaultTextureID(TextureUsage::Usage usage)
{
	return DEFAULT_TEXTURES[usage];
}

id_t
GetDefaultShaderID(TextureUsage::Usage usage)
{
	return DEFAULT_TEXTURES[usage];
}

EditorMaterial
GetDefaultEditorMaterial()
{
	EditorMaterial mat{};
	mat.Type = graphics::MaterialType::Opaque;
	mat.Surface = graphics::MaterialSurface{};
	mat.Flags = 0;
	mat.Name = "Default material";
	auto psvs{ content::GetDefaultPsVsShaders(true) };
	mat.ShaderIDs[0] = psvs.first;
	mat.ShaderIDs[1] = psvs.second;
	mat.TextureCount = TextureUsage::Count;
	for (u32 i{ 0 }; i < TextureUsage::Count; ++i) mat.TextureIDs[i] = DEFAULT_TEXTURES[i];
	return mat;
}

graphics::MaterialInitInfo
GetDefaultMaterialInitInfo()
{
	graphics::MaterialInitInfo info{};
	info.Type = graphics::MaterialType::Opaque;
	info.Surface = graphics::MaterialSurface{};
	info.TextureCount = TextureUsage::Count;
	info.TextureIDs = &DEFAULT_TEXTURES[0];
	auto psvs{ content::GetDefaultPsVsShaders(true) };
	info.ShaderIDs[0] = psvs.first;
	info.ShaderIDs[1] = psvs.second;
	return info;
}

void
RenderMaterialEditor()
{
	if (!isOpen) return;

	ImGui::Begin("Material View", nullptr);

	ImGui::Text("Type: %s", MATERIAL_SURFACE_TYPE_TO_STRING[materialInitInfo.Type]);
	ImGui::NextColumn();

	// imageButton for basecolor, normal, etc::

	DisplayTexture(TextureUsage::BaseColor, "Base Color:", "##Base");
	DisplayTexture(TextureUsage::Normal, "Normal:", "##Normal");
	DisplayTexture(TextureUsage::MetallicRoughness, "MetallicRoughness:", "##MetRou");
	DisplayTexture(TextureUsage::Emissive, "Emissive:", "##Emiss");
	DisplayTexture(TextureUsage::AmbientOcclusion, "Ambient Occlusion:", "##AO");

	DisplayMaterialSurfaceProperties(editorMaterial.Surface);

	if (ImGui::Button("Save"))
	{
		//TODO: or edit the material texture data itself
		id_t newMaterialID{ content::CreateMaterial(materialInitInfo) };
		//TODO: formal way
		ecs::component::RenderMaterial& mat{ ecs::scene::GetComponent<ecs::component::RenderMaterial>(materialOwner) };
		ecs::component::RenderMesh& mesh{ ecs::scene::GetComponent<ecs::component::RenderMesh>(materialOwner) };
		mat.MaterialIDs[0] = newMaterialID;

		//graphics::d3d12::core::RenderItemsUpdated();

		graphics::RemoveRenderItem(id::Index(materialOwner) - 1);
		graphics::AddRenderItem(materialOwner, mesh.MeshID, mat.MaterialCount, mat.MaterialIDs);
	}

	if (isBrowserOpen) RenderTextureBrowser();

	ImGui::End();
}


}