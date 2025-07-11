#include "EditorInterface.h"
#include "imgui.h"
#include "AssetBrowser.h"
#include "SceneEditorView.h"
#include "TextureView.h"
#include "MaterialEditor.h"
#include "ImporterView.h"

namespace mofu::editor {
namespace {


} // anonymous namespace



bool
InitializeEditorGUI()
{
    return InitializeSceneEditorView() && InitializeAssetBrowserGUI() && material::InitializeMaterialEditor();
}

void 
RenderEditorGUI()
{
    RenderSceneEditorView();
	RenderAssetBrowserGUI();
    texture::RenderTextureView();
    material::RenderMaterialEditor();
    RenderImportSummary();
}

}
