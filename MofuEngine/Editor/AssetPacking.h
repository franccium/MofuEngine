#pragma once
#include "CommonHeaders.h"
#include "Content/TextureImport.h"

namespace mofu::content {

void PackTextureForEditor(texture::TextureData& data, std::filesystem::path targetPath);
void PackTextureForEngine(texture::TextureData& data, std::filesystem::path targetPath);

}