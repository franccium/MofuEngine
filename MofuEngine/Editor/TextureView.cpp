#include "TextureView.h"
#include "Content/ResourceCreation.h"
#include "Content/ContentManagement.h"
#include "Graphics/UIRenderer.h"
#include "Content/TextureImport.h"
#include "Content/AssetImporter.h"
#include "ValueDisplay.h"

#include "imgui.h"

namespace mofu::editor::texture {
namespace {
id_t textureID{ id::INVALID_ID };
bool isOpen{ false };
ViewableTexture texture{};
ImTextureID imTextureID{};
u32 selectedSlice = 0;
u32 selectedMip = 0;

constexpr const char* DIMENSION_TO_STRING[content::texture::TextureDimension::Count]{
	"Texture 1D",
	"Texture 2D",
	"Texture 3D",
	"Cubemap",
};
constexpr const char* FORMAT_STRING{ "TODO" };

std::filesystem::path textureAssetFilePath{};

void
FillOutTextureViewData(std::filesystem::path textureAssetPath)
{
	textureAssetFilePath = textureAssetPath;
	std::unique_ptr<u8[]> buffer;
	u64 size{ 0 };
	content::ReadAssetFileNoVersion(textureAssetPath, buffer, size, content::AssetType::Texture);
	assert(buffer.get());

	util::BlobStreamReader reader{ buffer.get() };
	content::texture::TextureImportSettings& importSettings{ texture.ImportSettings };
	const char* filesBuffer{ reader.ReadStringWithLength() };
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

void
ReimportTexture()
{
	content::texture::TextureData data{};
	data.ImportSettings = texture.ImportSettings;
	
	content::ReimportTexture(data, textureAssetFilePath);

	OpenTextureView(textureAssetFilePath);
}

void 
RenderTextureWithConfig()
{
	ImGui::Begin("Texture View", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	u32 maxSlice = texture.ArraySize;
	u32 maxMip = texture.MipLevels;

	ImGui::Text("Slice:"); ImGui::SameLine();
	ImGui::SliderInt("##Slice", (int*)&selectedSlice, 0, maxSlice - 1);

	ImGui::Text("Mip:"); ImGui::SameLine();
	u32 mipLevel{ selectedMip };
	ImGui::SliderInt("##Mip", (int*)&selectedMip, 0, maxMip - 1);
	if (mipLevel != selectedMip)
	{
		imTextureID = mipLevel > 0 ? (ImTextureID)graphics::ui::GetImTextureID(textureID, mipLevel, texture.Format) 
			: (ImTextureID)graphics::ui::GetImTextureID(textureID);
	}

	content::texture::TextureImportSettings& settings{ texture.ImportSettings };
	ImGui::SeparatorText("Import Settings");
	ImGui::TextUnformatted("Files:");
	for (std::string_view s : content::SplitString(settings.Files, ';'))
	{
		ImGui::TextUnformatted(s.data());
	}
	if (ImGui::BeginCombo("Dimension", DIMENSION_TO_STRING[settings.Dimension]))
	{
		for (u32 i{ 0 }; i < content::texture::TextureDimension::Count; ++i)
		{
			const char* dimStr{ DIMENSION_TO_STRING[i] };
			bool chosen{ i == settings.Dimension };
			if (ImGui::Selectable(dimStr, chosen))
				settings.Dimension = (content::texture::TextureDimension::Dimension)i;
			if (chosen)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	DisplaySliderUint("Mip Levels", &settings.MipLevels, 0, 14);
	ImGui::SliderFloat("Alpha Threshold", &settings.AlphaThreshold, 0.f, 1.f);
	ImGui::TextUnformatted("Format: "); ImGui::SameLine();
	ImGui::TextUnformatted(FORMAT_STRING);
	DisplaySliderUint("Cubemap Size", &settings.CubemapSize, 16, 4096);

	ImGui::Checkbox("Prefer BC7", (bool*)&settings.PreferBC7);
	ImGui::Checkbox("Compress", (bool*)&settings.Compress);
	ImGui::Checkbox("Prefilter Cubemap", (bool*)&settings.PrefilterCubemap);
	ImGui::Checkbox("Mirror Cubemap", (bool*)&settings.MirrorCubemap);

	if (ImGui::Button("Reimport")) 
	{
		ReimportTexture();
	}

	ImageSlice* slice = &texture.Slices[selectedSlice][selectedMip];
	assert(slice && slice->RawContent);

	ImGui::SeparatorText("Texture Info");
	ImGui::Text("Dimensions: %ux%u", slice->Width, slice->Height);
	ImGui::Text("Row Pitch: %u", slice->RowPitch);
	ImGui::Text("Slice Pitch: %u", slice->SlicePitch);
	ImGui::Text("Format: 0x%X", texture.Format);

	ImGui::NextColumn();
	ImGui::SeparatorText("Preview");
	ImVec2 imageSize{ (f32)slice->Width, (f32)slice->Height };
	ImGui::Image(imTextureID, ImGui::GetContentRegionAvail(), { 0, 0 }, { 1, 1 });

	ImGui::End();
}

} // anonymous namespace

void
OpenTextureView(std::filesystem::path textureAssetPath)
{
	FillOutTextureViewData(textureAssetPath);

	//TODO: placeholder solution
	std::filesystem::path engineAssetPath{ textureAssetPath };
	engineAssetPath.replace_extension(".tex");
	textureID = content::CreateResourceFromAsset(engineAssetPath, content::AssetType::Texture);
	imTextureID = (ImTextureID)graphics::ui::GetImTextureID(textureID);
	assert(id::IsValid(textureID) && imTextureID);
	isOpen = true;
}

void
OpenTextureView(id_t textureID)
{
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

	RenderTextureWithConfig();
}

}
