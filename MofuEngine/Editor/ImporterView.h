#pragma once
#include "CommonHeaders.h"
#include <filesystem>
#include "Content/AssetImporter.h"
#include "Content/Asset.h"

namespace mofu::editor::assets {
void ViewFBXImportSummary(content::FBXImportState state);
void ViewImportSettings(content::AssetType::type assetType);
void ViewImportSettings(content::AssetHandle handle);
void RenderImportSummary();
void RenderImportSettings();

void RenderTextureImportSettings();

content::GeometryImportSettings GetGeometryImportSettings();
content::texture::TextureImportSettings GetTextureImportSettings();
}