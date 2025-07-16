#pragma once
#include "CommonHeaders.h"
#include "Content/Asset.h"

namespace mofu::editor {

enum class BrowserMode
{
	Assets = 0,
	AllFiles = 1
};

bool InitializeAssetBrowserGUI();
void RenderAssetBrowserGUI();

void AddNewRegisteredAsset(content::AssetHandle handle);
void DeleteRegisteredAsset(content::AssetHandle handle);
}