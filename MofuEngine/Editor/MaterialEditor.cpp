#include "MaterialEditor.h"
#include "Content/ContentManagement.h"
#include "imgui.h"
#include "Graphics/Renderer.h"
#include "Graphics/UIRenderer.h"

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

ecs::component::RenderMaterial material{};
graphics::MaterialInitInfo materialInitInfo{};

bool isOpen{ false };

[[nodiscard]] id_t
LoadTexture(const char* path)
{
	return content::CreateResourceFromAsset(path, content::AssetType::Texture);
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
OpenMaterialEditor(ecs::component::RenderMaterial mat)
{
	material = mat;
	materialInitInfo = graphics::GetMaterialReflection(mat.MaterialIDs[0]);
	isOpen = true;
}

void
OpenMaterialCreator(ecs::Entity entityID)
{
	material = {};
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
	constexpr ImVec2 imageSize{ 64,64 };

	ImGui::Begin("Material View", nullptr);

	ImGui::Text("Type: %s", MATERIAL_SURFACE_TYPE_TO_STRING[materialInitInfo.Type]);
	ImGui::NextColumn();

	// imageButton for basecolor, normal, etc::

	assert(id::IsValid(materialInitInfo.TextureIDs[TextureUsage::BaseColor]));

	ImGui::Text("Base Color:"); ImGui::SameLine();
	ImGui::ImageButton("##Base", graphics::ui::GetImTextureID(materialInitInfo.TextureIDs[TextureUsage::BaseColor]), imageSize);
	ImGui::NextColumn();
	ImGui::Text("Normal:"); ImGui::SameLine();
	ImGui::ImageButton("##Normal", graphics::ui::GetImTextureID(materialInitInfo.TextureIDs[TextureUsage::Normal]), imageSize);
	ImGui::NextColumn();
	ImGui::Text("MetallicRoughness:"); ImGui::SameLine();
	ImGui::ImageButton("##MetRou", graphics::ui::GetImTextureID(materialInitInfo.TextureIDs[TextureUsage::MetallicRoughness]), imageSize);
	ImGui::NextColumn();
	ImGui::Text("Emissive:"); ImGui::SameLine();
	ImGui::ImageButton("##Emiss", graphics::ui::GetImTextureID(materialInitInfo.TextureIDs[TextureUsage::Emissive]), imageSize);
	ImGui::NextColumn();
	ImGui::Text("Ambient Occlusion:"); ImGui::SameLine();
	ImGui::ImageButton("##AO", graphics::ui::GetImTextureID(materialInitInfo.TextureIDs[TextureUsage::AmbientOcclusion]), imageSize);

	ImGui::End();
}

}