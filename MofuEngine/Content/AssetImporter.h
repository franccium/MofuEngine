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
#include "External/ufbx/ufbx.h"

namespace mofu::content {

struct LightInitInfo
{
	v3 Color;
	f32 Intensity;
	graphics::light::LightType::Type Type;
	f32 Range;
	f32 InnerConeAngle;
	f32 OuterConeAngle;
	ecs::component::LocalTransform Transform;
	//u32 NodeIndex;
	std::string Name;
};

struct FBXLightNode
{
	ufbx_light* Light;
	u32 NodeIndex;
};;

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

	std::filesystem::path ModelResourcePath;
	std::filesystem::path ModelSourcePath;
	std::string Name;
	std::filesystem::path OutModelFile;
	//Array<FBXNode> Transforms;
	Array<FBXLightNode> FBXLightNodes;
	Vec<LodGroup> LodGroups;
	//Vec<ecs::component::LocalTransform> Transforms;
	Vec<editor::material::EditorMaterial> Materials;
	Vec<editor::texture::ViewableTexture> Textures;
	Vec<u32> SourceImages;
	Vec<std::string> ImageFiles;
	Vec<std::string> MeshNames;
	Vec<JPH::Ref<JPH::Shape>> JoltMeshShapes;
	Vec<AssetHandle> AllTextureHandles;

	Vec<LightInitInfo> Lights;
};

[[nodiscard]] AssetHandle ImportAsset(std::filesystem::path path, const std::filesystem::path& resourcePath);
void ImportAsset(AssetHandle handle);
void ReimportTexture(texture::TextureData& data, std::filesystem::path originalPath);

}