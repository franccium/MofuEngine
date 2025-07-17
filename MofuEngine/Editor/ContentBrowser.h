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

void AddRegisteredAsset(content::AssetHandle handle, content::AssetPtr asset);
void DeleteRegisteredAsset(content::AssetHandle handle);
}