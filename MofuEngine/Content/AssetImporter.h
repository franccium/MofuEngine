#pragma once
#include "CommonHeaders.h"
#include "ContentManagement.h"
#include <filesystem>
#include "TextureImport.h"

namespace mofu::content {


void ImportAsset(std::filesystem::path path);
void ReimportTexture(texture::TextureData& data, std::filesystem::path originalPath);


}