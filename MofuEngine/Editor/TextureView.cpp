#include "TextureView.h"
#include "Content/ResourceCreation.h"
#include "Content/ContentManagement.h"
#include "Graphics/UIRenderer.h"
#include "Content/TextureImport.h"
#include "Content/AssetImporter.h"
#include "ValueDisplay.h"
#include "ImporterView.h"
#include "Content/EditorContentManager.h"
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>

#include "imgui.h"

namespace mofu::editor::texture {
namespace {

struct Cubemap
{
	u32 ArrayIndex{ id::INVALID_ID };
	u32 MipIndex{ id::INVALID_ID };
	ImTextureID PosX;
	ImTextureID NegX;
	ImTextureID PosY;
	ImTextureID NegY;
	ImTextureID PosZ;
	ImTextureID NegZ;
};

id_t textureID{ id::INVALID_ID };
bool isOpen{ false };
ViewableTexture texture{};
content::AssetHandle textureAsset{ content::INVALID_HANDLE };
ImTextureID imTextureID{};
u32 arrayIndex{ 0 };
u32 mipIndex{ 0 };
u32 depthIndex{ 0 };
u32 maxArrayIndex{ 0 };
u32 maxMipIndex{ 0 };
u32 maxDepthIndex{ 0 };
Cubemap cubemap;
bool viewAsCubemap{ false };

std::filesystem::path textureAssetFilePath{};

void
SetCubemap()
{
	u32 index{ arrayIndex * 6 };
	if (index != cubemap.ArrayIndex || mipIndex != cubemap.MipIndex)
	{
		assert(index * 5 <= texture.ArraySize);
		cubemap = {index, mipIndex, 
		graphics::ui::GetImTextureID(textureID, 0, mipIndex, depthIndex, texture.Format, viewAsCubemap),
		graphics::ui::GetImTextureID(textureID, index + 1, mipIndex, depthIndex, texture.Format, viewAsCubemap), 
		graphics::ui::GetImTextureID(textureID, index + 2, mipIndex, depthIndex, texture.Format, viewAsCubemap), 
		graphics::ui::GetImTextureID(textureID, index + 3, mipIndex, depthIndex, texture.Format, viewAsCubemap), 
		graphics::ui::GetImTextureID(textureID, index + 4, mipIndex, depthIndex, texture.Format, viewAsCubemap), 
		graphics::ui::GetImTextureID(textureID, index + 5, mipIndex, depthIndex, texture.Format, viewAsCubemap), };
	}
}

void
FillOutTextureViewData(const std::filesystem::path& textureAssetPath)
{
	textureAssetFilePath = textureAssetPath;
	std::unique_ptr<u8[]> buffer;
	u64 size{ 0 };
	content::ReadAssetFileNoVersion(textureAssetPath, buffer, size, content::AssetType::Texture);
	assert(buffer.get());

	util::BlobStreamReader reader{ buffer.get() };

	// Icon data
	u32 iconSize{ reader.Read<u32>() };
	reader.Skip(iconSize);

	content::texture::TextureImportSettings& importSettings{ texture.ImportSettings };
	const char* filesBuffer{ reader.ReadStringWithLength() };
	importSettings.Files = std::string{ filesBuffer };
	importSettings.FileCount = reader.Read<u32>();
	importSettings.Dimension = (content::texture::TextureDimension::Dimension)reader.Read<u32>();
	importSettings.MipLevels = reader.Read<u32>();
	texture.ImportSettings.AlphaThreshold = reader.Read<f32>();
	importSettings.OutputFormat = (DXGI_FORMAT)reader.Read<u32>();
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
	texture.IBLPairHandle = reader.Read<u64>();

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

	texture.Slices.resize(arraySize);
	for (u32 i{ 0 }; i < arraySize; ++i)
	{
		texture.Slices[i].resize(mipLevels);
		for (u32 j{ 0 }; j < mipLevels; ++j)
		{
			texture.Slices[i][j].resize(depthPerMipLevel[j]);
			for (u32 k{ 0 }; k < depthPerMipLevel[j]; ++k)
			{
				const u32 width{ reader.Read<u32>() };
				const u32 height{ reader.Read<u32>() };
				const u32 rowPitch{ reader.Read<u32>() };
				const u32 slicePitch{ reader.Read<u32>() };
				texture.Slices[i][j][k] = ImageSlice{width, height, rowPitch, slicePitch, new u8[slicePitch]};
				reader.ReadBytes(texture.Slices[i][j][k].RawContent, slicePitch);
			}
		}
	}

	viewAsCubemap = texture.Flags & content::TextureFlags::IsCubeMap;

	maxArrayIndex = texture.Slices.size() - 1;
	maxMipIndex = texture.Slices[0].size() - 1;
	maxDepthIndex = texture.Slices[0][0].size() - 1;

	if (viewAsCubemap) maxArrayIndex /= 6;
}

void
ReimportTexture()
{
	assert(false); // TODO:
	content::texture::TextureData data{};
	//data.ImportSettings = texture.ImportSettings;
	data.ImportSettings = assets::GetTextureImportSettings();

}

void 
RenderTextureWithConfig()
{
	ImGui::Begin("Texture View", &isOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImGui::Text("Image:"); ImGui::SameLine();
	bool indicesChanged{ false };
	u32 array{ arrayIndex };
	ImGui::SliderInt("##Image", (int*)&array, 0, maxArrayIndex);
	if (array != arrayIndex)
	{
		indicesChanged = true;
		imTextureID = graphics::ui::GetImTextureID(textureID, array, mipIndex, depthIndex, texture.Format, viewAsCubemap);
		arrayIndex = array;
	}

	ImGui::Text("Mip:"); ImGui::SameLine();
	u32 mip{ mipIndex };
	ImGui::SliderInt("##Mip", (int*)&mip, 0, maxMipIndex);
	if (mip != mipIndex)
	{
		indicesChanged = true;
		imTextureID = graphics::ui::GetImTextureID(textureID, arrayIndex, mip, depthIndex, texture.Format, viewAsCubemap);
		mipIndex = mip;
	}

	u32 depth{ depthIndex };
	ImGui::Text("Depth:"); ImGui::SameLine();
	ImGui::SliderInt("##Depth", (int*)&depth, 0, maxDepthIndex);
	if (depth != depthIndex)
	{
		indicesChanged = true;
		imTextureID = graphics::ui::GetImTextureID(textureID, arrayIndex, mipIndex, depth, texture.Format, viewAsCubemap);
	}

	if (content::IsValid(texture.IBLPairHandle))
	{
		if (ImGui::Button("Display IBL Pair"))
		{
			OpenTextureView(texture.IBLPairHandle);
		}
	}

	assets::RenderTextureImportSettings();

	if (ImGui::Button("Reimport")) 
	{
		ReimportTexture();
	}

	ImageSlice* slice = &texture.Slices[arrayIndex][mipIndex][depthIndex];
	assert(slice && slice->RawContent);

	ImGui::SeparatorText("Texture Info");
	ImGui::Text("Dimensions: %ux%u", slice->Width, slice->Height);
	ImGui::Text("Row Pitch: %u", slice->RowPitch);
	ImGui::Text("Slice Pitch: %u", slice->SlicePitch);
	ImGui::Text("Format: %s", TEXTURE_FORMAT_STRING[texture.Format]);

	ImGui::NextColumn();
	ImGui::SeparatorText("Preview");
	
	if (viewAsCubemap)
	{
		if (indicesChanged) SetCubemap();
		//TODO: cube array 
		ImVec2 avail{ ImGui::GetContentRegionAvail() };
		f32 cellWidth{ avail.x / 4 };
		f32 cellHeight{ avail.y / 3 };
		ImVec2 cellSize{ cellWidth, cellHeight };

		ImGui::BeginTable("Cubemap Display", 4, ImGuiTableFlags_SizingFixedFit);
		for (u32 i{ 0 }; i < 4; ++i)
		{
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, cellWidth);
		}

		ImGui::TableNextRow(ImGuiTableRowFlags_None, cellHeight);
		ImGui::TableSetColumnIndex(1);
		ImGui::Image(cubemap.PosY, cellSize);
		
		ImGui::TableNextRow(ImGuiTableRowFlags_None, cellHeight);
		ImGui::TableNextColumn();
		ImGui::Image(cubemap.NegX, cellSize);

		ImGui::TableNextColumn();
		ImGui::Image(cubemap.PosZ, cellSize);

		ImGui::TableNextColumn();
		ImGui::Image(cubemap.PosX, cellSize);

		ImGui::TableNextColumn();
		ImGui::Image(cubemap.NegZ, cellSize);

		ImGui::TableNextRow(ImGuiTableRowFlags_None, cellHeight);
		ImGui::TableSetColumnIndex(1);
		ImGui::Image(cubemap.NegY, cellSize);

		ImGui::EndTable();
	}
	else
	{
		ImVec2 imageSize{ (f32)slice->Width, (f32)slice->Height };
		ImGui::Image(imTextureID, ImGui::GetContentRegionAvail(), { 0, 0 }, { 1, 1 });
	}

	ImGui::End();
}

} // anonymous namespace

void
ReleaseResources()
{
	//TODO: do i need to, they are deleted automatically by the creator
	if(id::IsValid(textureID))
		graphics::ui::DestroyViewTexture(textureID);
}

void
OpenTextureView(content::AssetHandle handle)
{
	ReleaseResources();

	mipIndex = 0;
	arrayIndex = 0;
	depthIndex = 0;
	viewAsCubemap = false;
	cubemap.ArrayIndex = id::INVALID_ID;

	content::AssetPtr asset{ content::assets::GetAsset(handle) };
	assert(asset);
	std::filesystem::path metadataPath{ asset->ImportedFilePath };
	metadataPath.replace_extension(".mt");
	FillOutTextureViewData(metadataPath);

	textureID = content::assets::GetResourceFromAsset(handle, content::AssetType::Texture);
	imTextureID = graphics::ui::GetImTextureID(textureID, arrayIndex, mipIndex, depthIndex, texture.Format, viewAsCubemap);
	assert(id::IsValid(textureID) && imTextureID);

	if (viewAsCubemap)
	{
		SetCubemap();
	}

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
