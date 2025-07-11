#pragma once
#include "CommonHeaders.h"
#include <filesystem>
#include "Content/AssetImporter.h"

namespace mofu::editor {
void ViewFBXImportSummary(content::FBXImportState state);
void RenderImportSummary();
}