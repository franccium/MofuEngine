#pragma once
#include "CommonHeaders.h"
#include "Content/TextureImport.h"

namespace mofu::content {

void PackTextureForEditor(const texture::TextureData& data, std::filesystem::path targetPath);
void PackTextureForEngine(const texture::TextureData& data, std::filesystem::path targetPath);
void SaveIcon(const texture::TextureData& data, std::filesystem::path targetPath);
}