#include "Material.h"
#include "Utilities/IOStream.h"
#include "Content/ContentUtils.h"
#include "Content/ResourceCreation.h"
#include "Content/EditorContentManager.h"
#include "Utilities/Logger.h"

namespace mofu::editor::material {
u64
GetMaterialEnginePackedSize(const EditorMaterial& material)
{
	constexpr u64 su32{ sizeof(u32) };

	u64 size{ sizeof(graphics::MaterialSurface) };

	size += su32 + TextureUsage::Count * sizeof(content::AssetHandle); // texture handles
	size += su32; // type
	size += graphics::ShaderType::Count * sizeof(content::AssetHandle); // shader handles
	size += su32; // flags

	return size;
}

// NOTE: intended only for the editor, this is not an engine-ready blob
void
PackMaterialAsset(const EditorMaterial& material, const std::filesystem::path& targetPath)
{
	const u64 dataSize{ GetMaterialEnginePackedSize(material) };
	u8* buffer{ new u8[dataSize] };
	u32 bufferSize{ (u32)dataSize };

	//TODO: more info like blending, layers etc

	util::BlobStreamWriter writer{ buffer, dataSize };

	const graphics::MaterialSurface& surface{ material.Surface };

	writer.Write<u32>(material.Type);
	writer.Write<u32>(material.Flags);
	writer.WriteVector<f32>(&surface.BaseColor.x, 4);
	writer.WriteVector<f32>(&surface.Emissive.x, 3);
	writer.Write<f32>(surface.EmissiveIntensity);
	writer.Write<f32>(surface.AmbientOcclusion);
	writer.Write<f32>(surface.Metallic);
	writer.Write<f32>(surface.Roughness);

	writer.Write<u32>(material.TextureCount);
	for (u32 i{ 0 }; i < material.TextureCount; ++i)
	{
		content::AssetHandle handle{ content::assets::GetAssetFromResource(material.TextureIDs[i], content::AssetType::Texture) };
		//if (id::IsValid(material.TextureIDs[i]) && !content::IsValid(handle))
		//{
		//	//NOTE: temporary fix, not sure if i need this
		//}
		assert(content::IsValid(handle));
		writer.Write<u64>(handle.id);
	}
	for (u32 i{ 0 }; i < graphics::ShaderType::Count; ++i)
	{
		//TODO:
		content::AssetHandle handle{ content::INVALID_HANDLE };
		/*content::AssetHandle handle{ content::assets::GetAssetFromResource(material.ShaderIDs[i], content::AssetType::Shader) };
		assert(content::IsValid(handle));*/
		writer.Write<u64>(handle.id);
	}

	assert(targetPath.extension() == ".mat");
	std::ofstream file{ targetPath, std::ios::out | std::ios::binary };
	if (!file) return;

	file.write(reinterpret_cast<const char*>(buffer), bufferSize);
	file.close();

	CreateMaterialAsset(targetPath); // TODO: standardise stuff like this
}

void
LoadMaterialAsset(EditorMaterial& outMaterial, const std::filesystem::path& path)
{
	std::unique_ptr<u8[]> buffer{};
	u64 size{};
	content::ReadAssetFileNoVersion(path, buffer, size, content::AssetType::Material);
	assert(buffer.get());

	util::BlobStreamReader reader{ buffer.get() };

	graphics::MaterialSurface& surface{ outMaterial.Surface };

	outMaterial.Type = (graphics::MaterialType::type)reader.Read<u32>();
	outMaterial.Flags = reader.Read<u32>();
	reader.ReadVector<f32>(&surface.BaseColor.x, 4);
	reader.ReadVector<f32>(&surface.Emissive.x, 3);
	surface.EmissiveIntensity = reader.Read<f32>();
	surface.AmbientOcclusion = reader.Read<f32>();
	surface.Metallic = reader.Read<f32>();
	surface.Roughness = reader.Read<f32>();

	outMaterial.TextureCount = reader.Read<u32>();
	for (u32 i{ 0 }; i < outMaterial.TextureCount; ++i)
	{
		content::AssetHandle handle{ reader.Read<u64>() };

		if (content::assets::GetAsset(handle))
		{
			id_t texID{ content::assets::CreateResourceFromHandle(handle) };
			assert(id::IsValid(texID));
			outMaterial.TextureIDs[i] = texID;
		}
		else
		{
			log::Error("Cant find texture asset for material [Handle:%u]", handle);
		}
	}

	log::Warn("Shader serialization is TODO");
	bool textured{ outMaterial.TextureCount != 0 };
	std::pair<id_t, id_t> vsps{ content::GetDefaultPsVsShadersTextured() };
	outMaterial.ShaderIDs[graphics::ShaderType::Vertex] = vsps.first;
	outMaterial.ShaderIDs[graphics::ShaderType::Pixel] = vsps.second;

	for (u32 i{ 0 }; i < graphics::ShaderType::Count; ++i)
	{
		/*content::AssetHandle handle{ reader.Read<u64>() };

		if (!content::assets::GetAsset(handle))
		{
			log::Error("Cant find shader asset for material [Handle:%u]", handle);
		}*/
	}
}

void
CreateMaterialAsset(const std::filesystem::path& path)
{
	assert(std::filesystem::exists(path));
	content::Asset* asset = new content::Asset{ content::AssetType::Material, path, path };
	content::assets::RegisterAsset(asset);
}

void
LoadMaterialDataFromAsset(graphics::MaterialInitInfo& outMaterial, content::AssetHandle materialAsset)
{
	EditorMaterial mat{};
	content::AssetPtr asset{ content::assets::GetAsset(materialAsset) };
	LoadMaterialAsset(mat, asset->ImportedFilePath);
	outMaterial.Type = mat.Type;
	outMaterial.Surface = mat.Surface;
	outMaterial.TextureCount = mat.TextureCount;
	memcpy(outMaterial.ShaderIDs, mat.ShaderIDs, graphics::ShaderType::Count * sizeof(id_t));
	outMaterial.TextureIDs = new id_t[mat.TextureCount];
	memcpy(outMaterial.TextureIDs, mat.TextureIDs, mat.TextureCount * sizeof(id_t));
}

}