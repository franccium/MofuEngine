#pragma once
#include "CommonHeaders.h"
#include "Graphics/Renderer.h"
#include "Content/Asset.h"
#include <filesystem>

namespace mofu::editor::material {
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

struct EditorMaterial
{
	std::string Name;
	u32 TextureIDs[TextureUsage::Count]{ id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID };
	graphics::MaterialSurface Surface;
	graphics::MaterialType::type Type;
	u32 TextureCount;
	id_t ShaderIDs[graphics::ShaderType::Count]{ id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID };
	u32 Flags;
};


// NOTE: intended only for the editor, this is not an engine-ready blob
void PackMaterialAsset(const EditorMaterial& material, const std::filesystem::path& targetPath);
void LoadMaterialAsset(EditorMaterial& outMaterial, const std::filesystem::path& path);
content::AssetHandle CreateMaterialAsset(const std::filesystem::path& path);
void LoadMaterialDataFromAsset(graphics::MaterialInitInfo& outMaterial, content::AssetHandle materialAsset);

}
