#pragma once
#include "CommonHeaders.h"
#include "Asset.h"
#include "Utilities/IOStream.h"
#include <filesystem>

namespace mofu::content::assets {
bool IsAssetAlreadyRegistered(const std::filesystem::path& path);
bool IsHandleValid(AssetHandle handle);
AssetPtr GetAsset(AssetHandle handle);
AssetHandle RegisterAsset(AssetPtr asset);

void InitializeAssetRegistry();
void ShutdownAssetRegistry();

}