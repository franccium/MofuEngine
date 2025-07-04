#include "TextureView.h"
#include "Content/ResourceCreation.h"
#include "Content/ContentManagement.h"
#include "Graphics/UIRenderer.h"
#include "Content/TextureImport.h"

#include "imgui.h"

namespace mofu::editor {
namespace {
id_t textureID{ id::INVALID_ID };
bool isOpen{ false };
ViewableTexture texture{};
ImTextureID imTextureID{};

void
FillOutTextureViewData(std::filesystem::path textureAssetPath)
{
	std::unique_ptr<u8[]> buffer;
	u64 size{ 0 };
	content::ReadAssetFileNoVersion(textureAssetPath, buffer, size, content::AssetType::Texture);
	assert(buffer.get());

	util::BlobStreamReader reader{ buffer.get() };
	content::texture::TextureImportSettings& importSettings{ texture.ImportSettings };
	u32 filesLength{ reader.Read<u32>() };
	char* filesBuffer{ (char*)_alloca(filesLength) };
	reader.ReadBytes((u8*)filesBuffer, filesLength);
	importSettings.Files = std::string{ filesBuffer };
	importSettings.FileCount = reader.Read<u32>();
	importSettings.Dimension = (content::texture::TextureDimension::Dimension)reader.Read<u32>();
	importSettings.MipLevels = reader.Read<u32>();
	texture.ImportSettings.AlphaThreshold = reader.Read<f32>();
	importSettings.OutputFormat = (DXGI_FORMAT)reader.Read<u32>(); //FIXME: unknown 
	importSettings.CubemapSize = reader.Read<u32>();
	importSettings.PreferBC7  = reader.Read<u8>();
	importSettings.Compress = reader.Read<u8>();
	importSettings.PrefilterCubemap = reader.Read<u8>();
	importSettings.MirrorCubemap = reader.Read<u8>();

	texture.Width = reader.Read<u32>();
	texture.Height = reader.Read<u32>();
	texture.ArraySize = reader.Read<u32>();
	texture.Flags = reader.Read<u32>();
	texture.MipLevels = reader.Read<u32>();
	texture.Format = (DXGI_FORMAT)reader.Read<u32>();
	reader.Skip(sizeof(u32)); //TODO: IBLPairGUID

	u32 depthPerMipLevel[content::texture::TextureData::MAX_MIPS];
	for (u32 i{ 0 }; i < content::texture::TextureData::MAX_MIPS; ++i)
	{
		depthPerMipLevel[i] = 1;
	}

	u32 arraySize{ texture.ArraySize };
	u32 mipLevels{ texture.MipLevels };
	bool is3D{ (texture.Flags & content::TextureFlags::IsVolumeMap) != 0 };
	if (is3D)
	{
		u32 depth = arraySize;
		arraySize = 1;
		u32 depthPerMip{ depth };
		for (u32 i{ 0 }; i < mipLevels; ++i)
		{
			depthPerMipLevel[i] = depthPerMip;
			depthPerMip = std::max(depthPerMip >> 1, (u32)1);
		}
	}

	texture.Slices = new ImageSlice*[arraySize];
	for (u32 i{ 0 }; i < arraySize; ++i)
	{
		texture.Slices[i] = new ImageSlice[mipLevels];
		for (u32 j{ 0 }; j < mipLevels; ++j)
		{
			const u32 width{ reader.Read<u32>() };
			const u32 height{ reader.Read<u32>() };
			const u32 rowPitch{ reader.Read<u32>() };
			const u32 slicePitch{ reader.Read<u32>() };
			texture.Slices[i][j] = ImageSlice{ width, height, rowPitch, slicePitch, new u8[slicePitch] };
			reader.ReadBytes(texture.Slices[i][j].RawContent, slicePitch);
			//TODO: reader.Skip(slicePitch * depthPerMipLevel[j]);
		}
	}

}

} // anonymous namespace

void
OpenTextureView(std::filesystem::path textureAssetPath)
{
	std::filesystem::path editorAsset{ "Assets/Generated/testTextureEditorPacked.tex" }; //TODO: placeholder
	FillOutTextureViewData(editorAsset);

	textureID = content::CreateResourceFromAsset(textureAssetPath, content::AssetType::Texture);
	imTextureID = (ImTextureID)graphics::ui::GetImTextureID(textureID);
	assert(id::IsValid(textureID) && imTextureID);
	isOpen = true;
}

void
RenderTextureView()
{
	if (!isOpen) return;

	assert(id::IsValid(textureID) && imTextureID);
	//graphics::ui::ViewTexture(textureID);

	ImGui::Begin("Texture View", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImGui::Image(imTextureID, ImGui::GetContentRegionAvail(), { 0, 0 }, { 1, 1 });
	ImGui::End();
}

}
