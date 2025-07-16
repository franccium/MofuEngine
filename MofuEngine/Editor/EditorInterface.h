#pragma once
#include "CommonHeaders.h"
#include "Content/Asset.h"

namespace mofu::editor
{

// TODO: figure out where these make sense
bool InitializeSceneEditorView(); 
bool InitializeEditorGUI();
void RenderEditorGUI();

void InspectAsset(content::AssetHandle handle);

}