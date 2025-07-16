#include "EditorInterface.h"
#include "imgui.h"
#include "ContentBrowser.h"
#include "SceneEditorView.h"
#include "TextureView.h"
#include "MaterialEditor.h"
#include "ImporterView.h"
#include "Content/EditorContentManager.h"
#include "AssetInteraction.h"

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
    assets::RenderImportSettings();
    assets::RenderImportSummary();
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
        texture::OpenTextureView(asset->ImportedFilePath.replace_extension(".etex")); //TODO: what to do with the editor files finally
        break;
    }
}

}
