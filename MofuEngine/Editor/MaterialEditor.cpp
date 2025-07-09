#include "MaterialEditor.h"
#include "Content/ContentManagement.h"
#include "imgui.h"
#include "Graphics/Renderer.h"
#include "Graphics/UIRenderer.h"
#include "TextureView.h"
#include "ValueDisplay.h"
#include "EngineAPI/ECS/SceneAPI.h"

#include "Graphics/D3D12/D3D12Core.h"

namespace mofu::editor::material {
namespace {
struct TextureUsage
{
	enum Usage : u32
	{
		BaseColor = 0,
		Normal,
		MetallicRoughness,
		Emissive,
		AmbientOcclusion,

		Count
	};
};
constexpr const char* WHITE_TEXTURE{ "Assets/Generated/Textures/white_placeholder_texture.tex" };
constexpr const char* GRAY_TEXTURE{ "Assets/Generated/Textures/gray_placeholder_texture.tex" };
constexpr const char* BLACK_TEXTURE{ "Assets/Generated/Textures/black_placeholder_texture.tex" };
constexpr const char* ERROR_TEXTURE{ "Assets/Generated/Textures/eror_texture.tex" };
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
graphics::MaterialInitInfo materialInitInfo{};

bool isOpen{ false };

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
		/*u32 val{ materialInitInfo.TextureIDs[texUse] };
		DisplayEditableUint(&materialInitInfo.TextureIDs[texUse], "Texture ID");
		if (materialInitInfo.TextureIDs[texUse] != val)
		{
		}*/
		materialInitInfo.TextureIDs[texUse] = 1;
	}
	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiButtonFlags_MouseButtonRight))
	{
		texture::OpenTextureView(materialInitInfo.TextureIDs[texUse]);
	}
}

} // anonymous namespace

bool 
InitializeMaterialEditor()
{
	for (u32 i{ 0 }; i < TextureUsage::Count; ++i)
	{
		//DEFAULT_TEXTURES[i] = LoadTexture(DEFAULT_TEXTURES_PATHS[i]);
		//assert(id::IsValid(DEFAULT_TEXTURES[i]));
	}
	return true;
}

void
OpenMaterialEditor(ecs::Entity entityID, ecs::component::RenderMaterial mat)
{
	materialOwner = entityID;
	material = mat;
	materialInitInfo = graphics::GetMaterialReflection(mat.MaterialIDs[0]);
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

void
RenderMaterialEditor()
{
	if (!isOpen) return;

	ImGui::Begin("Material View", nullptr);

	ImGui::Text("Type: %s", MATERIAL_SURFACE_TYPE_TO_STRING[materialInitInfo.Type]);
	ImGui::NextColumn();

	// imageButton for basecolor, normal, etc::

	assert(id::IsValid(materialInitInfo.TextureIDs[TextureUsage::BaseColor]));

	for (u32 i{ 0 }; i < TextureUsage::Count; ++i)
	{
		//
	}
	DisplayTexture(TextureUsage::BaseColor, "Base Color:", "##Base");
	DisplayTexture(TextureUsage::Normal, "Normal:", "##Normal");
	DisplayTexture(TextureUsage::MetallicRoughness, "MetallicRoughness:", "##MetRou");
	DisplayTexture(TextureUsage::Emissive, "Emissive:", "##Emiss");
	DisplayTexture(TextureUsage::AmbientOcclusion, "Ambient Occlusion:", "##AO");

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

	ImGui::End();
}

}