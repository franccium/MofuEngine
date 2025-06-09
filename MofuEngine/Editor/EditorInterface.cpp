#include "EditorInterface.h"
#include "imgui.h"
#include "AssetBrowser.h"
#include "SceneEditorView.h"

namespace mofu::editor {
namespace {


} // anonymous namespace



bool
InitializeEditorGUI()
{
    return InitializeSceneEditorView() && InitializeAssetBrowserGUI();
}

void 
RenderEditorGUI()
{
    RenderSceneEditorView();
	RenderAssetBrowserGUI();
}

}
