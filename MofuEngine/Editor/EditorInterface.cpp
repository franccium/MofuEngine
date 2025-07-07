#include "EditorInterface.h"
#include "imgui.h"
#include "AssetBrowser.h"
#include "SceneEditorView.h"
#include "TextureView.h"
#include "MaterialEditor.h"

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
    RenderTextureView();
    material::RenderMaterialEditor();
}

}
