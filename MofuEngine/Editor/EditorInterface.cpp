#include "EditorInterface.h"
#include "imgui.h"
#include "ContentBrowser.h"
#include "SceneEditorView.h"
#include "TextureView.h"
#include "MaterialEditor.h"
#include "ParticleEditor.h"
#include "ImporterView.h"
#include "Content/EditorContentManager.h"
#include "AssetInteraction.h"
#include "Content/Shaders/ContentProcessingShaders.h"
#include "ActionHistory.h"
#include "Input/InputSystem.h"
#include "RenderingConsole.h"
#include "ObjectAddInterface.h"
#include "ObjectPicker.h"
#include "Text/FontView.h"

namespace mofu::editor {
namespace {


} // anonymous namespace

bool
InitializeEditorGUI()
{
    object::Initialize();
    font::Initialize();
    debug::InitializeRenderingConsole();
    return InitializeSceneEditorView() && material::InitializeMaterialEditor() && particles::InitializeParticleEditor();
}

void 
RenderEditorGUI()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Assets"))
        {
            if (ImGui::MenuItem("Font Viewer")) font::OpenFontView();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools"))
        {
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    RenderSceneEditorView();
	RenderAssetBrowserGUI();
    texture::RenderTextureView();
    material::RenderMaterialEditor();
    particles::RenderParticleEditor();
    assets::RenderImportSettings();
    assets::RenderImportSummary();
    debug::DrawRenderingConsole();
    object::RenderObjectAddInterface();
    font::DisplayFontView();

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
