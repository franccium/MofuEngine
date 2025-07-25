#pragma once
#include "CommonHeaders.h"
#include "Asset.h"
#include "Utilities/IOStream.h"
#include <filesystem>
#include "ECS/Transform.h"

namespace mofu::content::assets {
bool IsAssetAlreadyRegistered(const std::filesystem::path& path);
bool IsHandleValid(AssetHandle handle);
[[nodiscard]] AssetHandle GetHandleFromPath(const std::filesystem::path& path);
[[nodiscard]] AssetHandle GetHandleFromImportedPath(const std::filesystem::path& path);
[[nodiscard]] AssetPtr GetAsset(AssetHandle handle);
AssetHandle RegisterAsset(AssetPtr asset);
AssetHandle RegisterAsset(AssetPtr asset, u64 id);

void PairAssetWithResource(AssetHandle handle, id_t resourceID, AssetType::type type);
[[nodiscard]] AssetHandle GetAssetFromResource(id_t resourceID, AssetType::type type);
[[nodiscard]] id_t GetResourceFromAsset(AssetHandle handle, AssetType::type type, bool createIfNotFound = true);
[[nodiscard]] Vec<id_t> GetResourcesFromAsset(AssetHandle handle, AssetType::type type);

id_t CreateResourceFromHandle(AssetHandle handle);

void ImportAllNotImported(AssetType::type type);
// TODO: this is useless, only need it to register all materials
void RegisterAllAssetsOfType(AssetType::type type);

void LoadEditorAssets();
void SaveAssetRegistry();
void InitializeAssetRegistry();
void ShutdownAssetRegistry();

void GetTextureMetadata(const std::filesystem::path& path, u64& outTextureSize, std::unique_ptr<u8[]>& textureBuffer,
	u64& outIconSize, std::unique_ptr<u8[]>& iconBuffer);
void GetTextureIconData(const std::filesystem::path& path, u64& outIconSize, std::unique_ptr<u8[]>& iconBuffer);

void ParseMetadata(AssetPtr asset);

void LoadMeshAsset(AssetHandle asset, ecs::Entity entity, ecs::component::RenderMesh& mesh, ecs::component::RenderMaterial& material);

const content::AssetHandle DEFAULT_MESH_HANDLE{ 10 };
const content::AssetHandle DEFAULT_MATERIAL_UNTEXTURED_HANDLE{ 11 };
const content::AssetHandle DEFAULT_BRDF_LUT_HANDLE{ 14 };

}