#pragma once
#include "CommonHeaders.h"
#include <filesystem>
#include "Content/AssetImporter.h"
#include "Content/Asset.h"

namespace mofu::editor::assets {
void ViewFBXImportSummary(std::unique_ptr<content::FBXImportState>& state);
void ViewImportSettings(content::AssetType::type assetType);
void ViewImportSettings(content::AssetHandle handle);
void AddFile(const std::string& filename);
void RefreshFiles(const Vec<std::string>& files);
void RenderImportSummary();
void RenderImportSettings();

void RenderTextureImportSettings();

const content::GeometryImportSettings& GetGeometryImportSettings();
const content::texture::TextureImportSettings& GetTextureImportSettings();
content::texture::TextureImportSettings& GetTextureImportSettingsA(); // TODO: temporary
}