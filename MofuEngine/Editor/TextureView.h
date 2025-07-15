#pragma once
#include "CommonHeaders.h"
#include <filesystem>
#include <array>
#include "Content/TextureImport.h"

namespace mofu::editor::texture
{
struct ImageSlice
{
	u32 Width{ 0 };
	u32 Height{ 0 };
	u32 RowPitch{ 0 };
	u32 SlicePitch{ 0 };
	u8* RawContent{ nullptr };
};

struct ViewableTexture
{
	content::texture::TextureImportSettings ImportSettings{};
	u32 Width{ 0 };
	u32 Height{ 0 };
	u32 ArraySize{ 0 };
	u32 MipLevels{ 0 };
	u32 Flags{ 0 };
	//TODO: IBLPairGUID
	DXGI_FORMAT Format{};
	ImageSlice** Slices{ nullptr };

	bool IsValid{ false };
};

constexpr const char* DIMENSION_TO_STRING[content::texture::TextureDimension::Count]{
	"Texture 1D",
	"Texture 2D",
	"Texture 3D",
	"Cubemap",
};
constexpr const char* FORMAT_STRING{ "TODO" };

void OpenTextureView(std::filesystem::path textureAssetPath);
void OpenTextureView(id_t textureID);
void RenderTextureView();
}