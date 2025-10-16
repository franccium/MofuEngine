#include "EditorInterface.h"
#include "imgui.h"
#include "ContentBrowser.h"
#include "SceneEditorView.h"
#include "TextureView.h"
#include "MaterialEditor.h"
#include "ImporterView.h"
#include "Content/EditorContentManager.h"
#include "AssetInteraction.h"
#include "Content/Shaders/ContentProcessingShaders.h"
#include "ActionHistory.h"
#include "Input/InputSystem.h"
#include "RenderingConsole.h"

namespace mofu::editor {
namespace {


} // anonymous namespace



bool
InitializeEditorGUI()
{
    return InitializeSceneEditorView() && material::InitializeMaterialEditor();
}

void 
RenderEditorGUI()
{
    RenderSceneEditorView();
	RenderAssetBrowserGUI();
    texture::RenderTextureView();
    material::RenderMaterialEditor();
    assets::RenderImportSettings();
    assets::RenderImportSummary();
    debug::DrawRenderingConsole();

    if (input::WasKeyPressed(input::Keys::Z, input::Keys::Control))
    {
        Undo();
    }
    else if (input::WasKeyPressed(input::Keys::X, input::Keys::Control))
    {
        Redo();
    }
}

void 
InspectAsset(content::AssetHandle handle)
{
    content::AssetPtr asset{ content::assets::GetAsset(handle) };
    switch (asset->Type)
    {
    case content::AssetType::Mesh:
        assets::DropModelIntoScene(asset->ImportedFilePath);
        break;
    case content::AssetType::Texture:
        texture::OpenTextureView(handle);
        break;
    case content::AssetType::Material:
        material::OpenMaterialView(handle);
        break;
    }
}

void
ShutdownEditorGUI()
{
    ShutdownSceneEditorView();
}

}
