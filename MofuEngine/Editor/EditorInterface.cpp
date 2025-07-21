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

}
