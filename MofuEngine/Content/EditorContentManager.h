#pragma once
#include "CommonHeaders.h"
#include "Asset.h"
#include "Utilities/IOStream.h"
#include <filesystem>

namespace mofu::content::editor {

void ReadAssetFile(std::filesystem::path path, std::unique_ptr<u8[]>& dataOut, u64& sizeOut, AssetType::type type);
void ReadAssetFileNoVersion(std::filesystem::path path, std::unique_ptr<u8[]>& dataOut, u64& sizeOut, AssetType::type type);
}