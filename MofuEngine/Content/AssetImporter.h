#pragma once
#include "CommonHeaders.h"
#include "ContentManagement.h"
#include <filesystem>
#include "TextureImport.h"
#include "Graphics/GeometryData.h"
#include "Editor/TextureView.h"
#include "Editor/MaterialEditor.h"

namespace mofu::content {
	
struct Color
{
	f32 r;
	f32 g;
	f32 b;
	f32 a{ 1.f };

	constexpr Color LinearToSRGB()
	{
		return { r < 0.0031308f ? 12.92f * r : f32((1.0 + 0.055) * std::pow(r, 1.0f / 2.4f) - 0.055),
			g < 0.0031308f ? 12.92f * g : f32((1.0 + 0.055) * std::pow(g, 1.0f / 2.4f) - 0.055),
			b < 0.0031308f ? 12.92f * b : f32((1.0 + 0.055) * std::pow(b, 1.0f / 2.4f) - 0.055),
			a };
	}

	constexpr Color SRGBToLinear()
	{
		return { r < 0.04045f ? r * (1.0f / 12.92f) : std::pow(f32((r + 0.055) * (1.0 / (1.0 + 0.055))), 2.4f),
			g < 0.04045f ? g * (1.0f / 12.92f) : std::pow(f32((g + 0.055) * (1.0 / (1.0 + 0.055))), 2.4f),
			b < 0.04045f ? b * (1.0f / 12.92f) : std::pow(f32((b + 0.055) * (1.0 / (1.0 + 0.055))), 2.4f),
			a };
	}
};

struct FBXImportState
{
	std::string FbxFile;
	std::string OutModelFile;
	Vec<LodGroup> LodGroups;
	Vec<editor::material::EditorMaterial> Materials;
	Vec<editor::texture::ViewableTexture> Textures;
	Vec<u32> SourceImages;
	Vec<std::string> ImageFiles;

	enum ErrorFlags : u32
	{
		None = 0x0,
		InvalidTexturePath = 0x1,
		MaterialTextureNotFound = 0x2,
		Test1 = 0x04,
		Test2 = 0x08,
		Test3 = 0x10,
		Test4 = 0x20,
	};
	u32 Errors;
};

void ImportAsset(std::filesystem::path path);
void ReimportTexture(texture::TextureData& data, std::filesystem::path originalPath);


}