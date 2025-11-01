#pragma once
#include "CommonHeaders.h"
#include "Asset.h"
#include "ContentManagement.h"
#include <filesystem>
#include "TextureImport.h"
#include "Graphics/GeometryData.h"
#include "Editor/TextureView.h"
#include "Editor/MaterialEditor.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

namespace mofu::content {


struct FBXImportState
{
	GeometryImportSettings ImportSettings;
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

	std::string FbxFile;
	std::filesystem::path OutModelFile;
	Vec<LodGroup> LodGroups;
	Vec<editor::material::EditorMaterial> Materials;
	Vec<editor::texture::ViewableTexture> Textures;
	Vec<u32> SourceImages;
	Vec<std::string> ImageFiles;
	Vec<std::string> MeshNames;
	Vec<JPH::Ref<JPH::Shape>> JoltMeshShapes;
};

void ImportAsset(std::filesystem::path path);
void ImportAsset(AssetHandle handle);
void ReimportTexture(texture::TextureData& data, std::filesystem::path originalPath);

}