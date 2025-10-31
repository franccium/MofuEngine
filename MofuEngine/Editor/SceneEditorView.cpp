#include "SceneEditorView.h"
#include "imgui.h"
#include "ECS/Component.h"
#include "ECS/TransformHierarchy.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "MaterialEditor.h"
#include "AssetInteraction.h"
#include "Project/Project.h"
#include "Content/EditorContentManager.h"

namespace mofu::editor {
namespace {
constexpr u32 NODE_NAME_LENGTH{ ecs::component::NAME_LENGTH };
// TODO: adjust this
struct EntityTreeNode
{
    char Name[NODE_NAME_LENGTH];
    ecs::Entity ID;
    EntityTreeNode* Parent = NULL;
    ImVector<EntityTreeNode*> Children;
    u16 IndexInParent = 0;
};

EntityTreeNode* _rootNode{ nullptr };
Vec<std::pair<ecs::Entity, EntityTreeNode*>> entityToPair{};

Vec<std::string> sceneFiles{};
Vec<std::string> prefabFiles{};
Vec<std::string> meshFiles{};
std::filesystem::path currentScenePath{};
EntityTreeNode* selectedNode{ nullptr };
char sceneNameBuffer[ecs::scene::Scene::MAX_NAME_LENGTH];


constexpr EntityTreeNode*
FindParentAsNode(ecs::Entity parentEntity)
{
    for (auto [e, n] : entityToPair)
    {
        if (e == parentEntity) return n;
    }
    assert(false);
    return _rootNode;
}

void
PrepareAddedComponent(ecs::ComponentID cid, ecs::Entity entity)
{
    switch (cid)
    {
    case ecs::component::ID<ecs::component::RenderMesh>:
    {
        if (!ecs::scene::HasComponent<ecs::component::WorldTransform>(entity))
        {
            ecs::scene::AddComponent<ecs::component::WorldTransform>(entity);
        }
        if (!ecs::scene::HasComponent<ecs::component::RenderMaterial>(entity))
        {
            ecs::scene::AddComponent<ecs::component::RenderMaterial>(entity);
            PrepareAddedComponent(ecs::component::ID<ecs::component::RenderMaterial>, entity);
        }
        ecs::component::RenderMesh& mesh{ ecs::scene::GetComponent<ecs::component::RenderMesh>(entity) };
        mesh.MeshID = content::GetDefaultMesh();
        mesh.MeshAsset = content::assets::DEFAULT_MESH_HANDLE;

        const ecs::component::RenderMaterial& mat{ ecs::scene::GetComponent<ecs::component::RenderMaterial>(entity) };

        mesh.RenderItemID = graphics::AddRenderItem(entity, mesh.MeshID, mat.MaterialCount, mat.MaterialID);

        break;
    }
    case ecs::component::ID<ecs::component::RenderMaterial>:
    {
        ecs::component::RenderMaterial& mat{ ecs::scene::GetComponent<ecs::component::RenderMaterial>(entity) };
        ecs::component::RenderMaterial material{};
        material.MaterialCount = 1;
        material.MaterialID = content::GetDefaultMaterial();
        material.MaterialAsset = content::assets::DEFAULT_MATERIAL_UNTEXTURED_HANDLE;
        mat = material;
        break;
    }
    default:
        break;
    }
}

void
CreateEntityTreeNode(ecs::Entity entity, EntityTreeNode* parentNode)
{
    EntityTreeNode* node = new EntityTreeNode{};
    node->ID = entity;
    if (ecs::scene::HasComponent<ecs::component::NameComponent>(entity))
    {
        const char* name{ ecs::scene::GetComponent<ecs::component::NameComponent>(entity).Name };
        snprintf(node->Name, NODE_NAME_LENGTH, "%s", name);
    }
    else
    {
       snprintf(node->Name, NODE_NAME_LENGTH, "E %u", (u32)entity);
    }
    node->Parent = parentNode;
    node->IndexInParent = (u16)parentNode->Children.Size;
    parentNode->Children.push_back(node);
    entityToPair.emplace_back(entity, node);
}

// TODO: can probably do it better
static void CreateSceneHierarchyTree(const Vec<ecs::EntityData>& entityData)
{
    _rootNode = new EntityTreeNode{};
	char rootName[NODE_NAME_LENGTH]{ "Root" };
    snprintf(_rootNode->Name, NODE_NAME_LENGTH, "%s", rootName);
    _rootNode->ID = ecs::Entity{ id::INVALID_ID };

    for (const ecs::EntityData& e : entityData)
    {
        CreateEntityTreeNode(e.id, _rootNode);
	}
}

void DeleteEntity(EntityTreeNode* node)
{
    //if (!node) return;
    //if (!selectedNode) return;
    assert(node);
    if (!node) return;
    ecs::Entity selected{ node->ID };
    //ecs::Entity selected{ selectedNode->ID };

    log::Error("Not Implemented");
}

void 
CreateEntity(EntityTreeNode* node)
{
    //ecs::Entity selected{ node ? node->ID : id::INVALID_ID };
    assert(node);
    if (!node) return;
    ecs::Entity selected{ node->ID };
	//ecs::Entity selected{ selectedNode ? selectedNode->ID : id::INVALID_ID };

    ecs::component::LocalTransform lt{};

    //TODO: cleaner
    const ecs::EntityData& entityData = id::IsValid(selected)
        ? ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::Child, ecs::component::WorldTransform>(
            lt, ecs::component::Child{ {}, selected }, {})
        : ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::Parent, ecs::component::WorldTransform>(lt, {}, {});

    if (!ecs::scene::EntityHasComponent<ecs::component::Parent>(selected))
    {
        ecs::scene::AddComponent<ecs::component::Parent>(selected);
    }

    AddEntityToSceneView(entityData.id);
}

void 
DuplicateEntity(EntityTreeNode* node)
{
    //if (!selectedNode) return;
    assert(node);
    if (!node) return;
    ecs::Entity selected{ node->ID };

    //ecs::Entity selected{ selectedNode ? selectedNode->ID : id::INVALID_ID };

    const ecs::EntityData& duplicatedData{ ecs::scene::GetEntityData(selected) };
    const ecs::EntityBlock* const duplicatedBlock{ duplicatedData.block };

    const ecs::EntityData& newData = ecs::scene::SpawnEntity(duplicatedBlock->Signature);
    const ecs::EntityBlock* const newBlock{ newData.block }; // in case the original block got filled, grab the one given

    const u32 duplicatedRow{ duplicatedData.row };
    const u32 newRow{ newData.row };

    // copy over component values
    for (ecs::ComponentID cid : newBlock->GetComponentView())
    {
        const u32 componentSize{ ecs::component::GetComponentSize(cid) };
        const u32 duplicatedOffset{ duplicatedBlock->ComponentOffsets[cid] + componentSize * duplicatedRow };
        const u32 newOffset{ newBlock->ComponentOffsets[cid] + componentSize * newRow };
        memcpy(newBlock->ComponentData + newOffset, duplicatedBlock->ComponentData + duplicatedOffset, componentSize);
    }

    const ecs::Entity newEntity{ newData.id };
    if (ecs::scene::HasComponent<ecs::component::RenderMesh>(newEntity))
    {
        ecs::component::RenderMesh& mesh{ ecs::scene::GetComponent<ecs::component::RenderMesh>(newEntity) };
        const ecs::component::RenderMaterial& material{ ecs::scene::GetComponent<ecs::component::RenderMaterial>(newEntity) };
        mesh.RenderItemID = graphics::AddRenderItem(newEntity, mesh.MeshID, material.MaterialCount, material.MaterialID);
    }

    AddEntityToSceneView(newEntity);
}

struct SceneHierarchy
{
    ImGuiTextFilter Filter;

    void
    DrawSceneSaveLoad()
    {
        ImGui::Text("Scene: %s", ecs::scene::GetCurrentScene().Name);
        if (ImGui::IsItemClicked())
        {
			ImGui::OpenPopup("RenameScenePopup");
			memcpy(sceneNameBuffer, ecs::scene::GetCurrentScene().Name, ecs::scene::Scene::MAX_NAME_LENGTH);
        }
        if (ImGui::IsPopupOpen("RenameScenePopup"))
        {
			ImGui::TextUnformatted("Name:");
            ImGui::SameLine();
			ImGui::PushID(&sceneNameBuffer);
            ImGui::InputText("", sceneNameBuffer, ecs::scene::Scene::MAX_NAME_LENGTH);
			ImGui::PopID();
            ImGui::TableNextColumn();
            ImGui::SameLine();
            if (ImGui::Button("Confirm"))
            {
                memcpy((void*)ecs::scene::GetCurrentScene().Name, sceneNameBuffer, ecs::scene::Scene::MAX_NAME_LENGTH);
				ImGui::CloseCurrentPopup();
                std::filesystem::path newScenePath;
                if (std::filesystem::exists(currentScenePath))
                {
                    newScenePath = currentScenePath;
				    newScenePath.replace_filename(sceneNameBuffer);
				    newScenePath.replace_extension(content::SCENE_FILE_EXTENSION);
                    std::filesystem::rename(currentScenePath, newScenePath);
                }
                else
                {
					newScenePath = project::GetSceneDirectory() / sceneNameBuffer;
					newScenePath += content::SCENE_FILE_EXTENSION;
                }
				currentScenePath = newScenePath;
            }
        }

        if (ImGui::Button("Save Scene"))
        {
            Vec<Vec<ecs::Entity>> hierarchies{};
            //TODO: more depth levels
            for (EntityTreeNode* parent : _rootNode->Children)
            {
                hierarchies.emplace_back();
                Vec<ecs::Entity>& hierarchy{ hierarchies.back() };
                hierarchy.emplace_back(parent->ID);
                for (EntityTreeNode* child : parent->Children)
                {
                    hierarchy.emplace_back(child->ID);
                }
            }
            assets::SerializeScene(ecs::scene::GetCurrentScene(), hierarchies);
        }
        if (ImGui::Button("Load Scene"))
        {
            sceneFiles.clear();
            content::ListFilesByExtensionRec(content::SCENE_FILE_EXTENSION, project::GetSceneDirectory(), sceneFiles);
            ImGui::OpenPopup("LoadScenePopup");
        }
        if (ImGui::BeginPopup("LoadScenePopup"))
        {
            for (std::string_view path : sceneFiles)
            {
                if (ImGui::Selectable(path.data()))
                {
                    LoadScene(path);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
    }

    void
    DrawEntityHierarchy()
    {
        // Left side: draw tree
        if (ImGui::BeginChild("##tree", ImVec2(250, 0), ImGuiChildFlags_ResizeX | ImGuiChildFlags_ResizeY | ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened))
        {
            // draw the filter field
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F, ImGuiInputFlags_Tooltip);
            ImGui::PushItemFlag(ImGuiItemFlags_NoNavDefaultFocus, true);
            if (ImGui::InputTextWithHint("##Filter", "incl,-excl", Filter.InputBuf, IM_ARRAYSIZE(Filter.InputBuf), ImGuiInputTextFlags_EscapeClearsAll))
                Filter.Build();
            ImGui::PopItemFlag();

            if (ImGui::BeginTable("##bg", 1, ImGuiTableFlags_RowBg))
            {
                for (EntityTreeNode* node : _rootNode->Children)
                    if (Filter.PassFilter(node->Name)) // Filter root node
                        DrawTreeNode(node);
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        // Right side: draw properties
        ImGui::SameLine();

        ImGui::BeginGroup(); // Lock X position
        EntityTreeNode* node = selectedNode;
        if (node)
        {
            ImGui::Text("%s", node->Name);
            ecs::Entity entity{ node->ID };
            ImGui::TextDisabled("UID: 0x%08X", entity);
            ImGui::Separator();

            if (ImGui::BeginTable("##properties", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
            {
                ImGui::PushID((int)entity);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 2.0f); // Default twice larger

                using namespace ecs;

                //TODO: make an iterator or a view
                const EntityData& entityData{ ecs::scene::GetEntityData(entity) };
                const EntityBlock* const block{ ecs::scene::GetEntityData(entity).block };

                ForEachComponent(block, entityData.row, [](ComponentID cid, u8* data) {
                    component::RenderLUT[cid](data);
                    });

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::Button("+ Add Component"))
                    ImGui::OpenPopup("AddComponentPopup");

                if (ImGui::BeginPopup("AddComponentPopup"))
                {
                    for (ComponentID cid{ 0 }; cid < component::ComponentTypeCount; ++cid)
                    {
                        if (!block->Signature.test(cid))
                        {
                            if (ImGui::Selectable(ecs::component::ComponentNames[cid]))
                            {
                                ecs::component::AddLUT[cid](entity);
                                PrepareAddedComponent(cid, entity);
                            }
                        }
                    }
                    ImGui::EndPopup();
                }

                if (ecs::scene::HasComponent<ecs::component::RenderMaterial>(entity))
                {
                    if (ImGui::Button("Edit Material"))
                    {
                        ecs::component::RenderMaterial mat{ ecs::scene::GetComponent<ecs::component::RenderMaterial>(entity) };
                        material::OpenMaterialEditor(entity, mat);
                    }
                }
                else
                {
                    if (ImGui::Button("Add Material"))
                    {
                        material::OpenMaterialCreator(entity);
                    }
                }
                if (ecs::scene::HasComponent<ecs::component::RenderMesh>(entity))
                {
                    //TODO: move this somewhere 
                    if (ImGui::Button("Change"))
                    {
                        ImGui::OpenPopup("ChangeMeshPopup");
                    }
                    if (ImGui::BeginPopup("ChangeMeshPopup"))
                    {
                        meshFiles.clear();
                        content::ListFilesByExtensionRec(".mesh", editor::project::GetResourceDirectory(), meshFiles);

                        for (std::string_view path : meshFiles)
                        {
                            if (ImGui::Selectable(path.data()))
                            {
                                content::AssetHandle assetHandle{ content::assets::GetHandleFromImportedPath(path) };
                                if (assetHandle != content::INVALID_HANDLE)
                                {
                                    ecs::component::RenderMesh& mesh{ ecs::scene::GetComponent<ecs::component::RenderMesh>(entity) };
                                    ecs::component::RenderMaterial& mat{ ecs::scene::GetComponent<ecs::component::RenderMaterial>(entity) };
                                    content::assets::LoadMeshAsset(assetHandle, entity, mesh, mat);
                                }
                                else
                                {
                                    log::Warn("Can't find Mesh assetHandle from path");
                                }
                                ImGui::CloseCurrentPopup();
                            }
                        }
                        ImGui::EndPopup();
                    }
                }

                if (ecs::scene::HasComponent<ecs::component::Parent>(entity))
                {
                    if (ImGui::Button("Save Hierarchy"))
                    {
                        Vec<ecs::Entity> entities{};
                        entities.emplace_back(entity);
                        for (auto c : node->Children)
                        {
                            entities.emplace_back(c->ID);
                        }
                        assets::SerializeEntityHierarchy(entities);
                    }
                }

                ImGui::PopID();
                ImGui::EndTable();
            }
        }
        ImGui::EndGroup();
    }

    void Draw()
    {
        ImGui::BeginGroup();
        DrawSceneSaveLoad();

        if (ImGui::Button("Import Hierarchy"))
        {
            prefabFiles.clear();
            content::ListFilesByExtensionRec(content::PREFAB_FILE_EXTENSION, project::GetPrefabDirectory(), prefabFiles);
            ImGui::OpenPopup("ImportHierarchyPopup");
        }
        if (ImGui::BeginPopup("ImportHierarchyPopup"))
        {
            for (std::string_view path : prefabFiles)
            {
                if (ImGui::Selectable(path.data()))
                {
                    AddPrefab(path);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
        if (ImGui::Button("Add Entity"))
        {
            ecs::component::LocalTransform transform{};
            ecs::EntityData& entityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, 
                ecs::component::Parent, ecs::component::WorldTransform>(transform, {}, {}) };
            AddEntityToSceneView(entityData.id);
        }
        ImGui::EndGroup();

        DrawEntityHierarchy();
    }

    void DrawTreeNode(EntityTreeNode* node)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushID(node->ID);
        ImGuiTreeNodeFlags tree_flags = ImGuiTreeNodeFlags_None;
        tree_flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        tree_flags |= ImGuiTreeNodeFlags_NavLeftJumpsBackHere; // Left arrow support
        tree_flags |= ImGuiTreeNodeFlags_SpanFullWidth; // Span full width for easier mouse reach
        tree_flags |= ImGuiTreeNodeFlags_DrawLinesToNodes; // Always draw hierarchy outlines
        if (node == selectedNode)
            tree_flags |= ImGuiTreeNodeFlags_Selected;
        if (node->Children.Size == 0)
            tree_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
        bool node_open{ ImGui::TreeNodeEx("", tree_flags, "%s", node->Name) };
        if (ImGui::IsItemFocused())
            selectedNode = node;
        if (node_open)
        {
            for (EntityTreeNode* child : node->Children)
                DrawTreeNode(child);
            ImGui::TreePop();
        }
        if (ImGui::BeginPopupContextItem("NodeContextMenu"))
        {
            if (ImGui::MenuItem("Create Child Entity"))
                CreateEntity(node);
            if (ImGui::MenuItem("Delete Entity"))
                DeleteEntity(node);
            if (ImGui::MenuItem("Duplicate Entity"))
                DuplicateEntity(node);
            ImGui::EndPopup();
        }
        
        ImGui::PopID();
    }
};

SceneHierarchy sceneHierarchy;

} // anonymous namespace

//TODO: instead of adding 1 by 1 add a whole bunch from tables of to add and to remove or sth?
void AddEntityToSceneView(ecs::Entity entity)
{
    assert(ecs::scene::IsEntityAlive(entity));

    EntityTreeNode* parentNode{ _rootNode };
    if (ecs::scene::HasComponent<ecs::component::Child>(entity))
    {
        ecs::component::Child p{ ecs::scene::GetComponent<ecs::component::Child>(entity) };
        parentNode = FindParentAsNode(p.ParentEntity);
    }

    CreateEntityTreeNode(entity, parentNode);


    //TODO: move this somewhere, for now its the only palce where i know the components are all initialized
    ecs::transform::AddEntityToHierarchy(entity);
}

bool 
InitializeSceneEditorView()
{
	memset(sceneNameBuffer, 0, sizeof(sceneNameBuffer));
    const Vec<ecs::EntityData>& entityData = ecs::scene::GetAllEntityData();
    for (const ecs::EntityData e : entityData)
    {
        log::Info("found entity: %u, block: %p, row: %u", e.id, e.block, e.row);
    }

    CreateSceneHierarchyTree(entityData);

    return true;
}

void RenderSceneEditorView()
{
    ImGui::Begin("Scene hierarchy");

    sceneHierarchy.Draw();

    ImGui::End();
}

void LoadScene(const std::filesystem::path& path)
{
    if(currentScenePath == path)
    {
        log::Warn("Scene already loaded");
        return;
	}
    Vec<Vec<ecs::Entity>> hierarchies{};
    assets::LoadScene(hierarchies, path);
    currentScenePath = path;
    entityToPair.clear();
    delete _rootNode;
    _rootNode = nullptr;
    selectedNode = nullptr;

    _rootNode = new EntityTreeNode{};
    char rootName[NODE_NAME_LENGTH]{ "Root" };
    snprintf(_rootNode->Name, NODE_NAME_LENGTH, "%s", rootName);
    _rootNode->ID = ecs::Entity{ id::INVALID_ID };
    //CreateSceneHierarchyTree(ecs::scene::GetAllEntityData());
    for (const Vec<ecs::Entity>& hierarchy : hierarchies)
    {
        for (const ecs::Entity e : hierarchy)
        {
            AddEntityToSceneView(e);
        }
	}
}

void 
ShutdownSceneEditorView()
{
	delete _rootNode;
	_rootNode = nullptr;
}

ecs::Entity AddPrefab(const std::filesystem::path& path)
{
    Vec<ecs::Entity> entities{};
    assets::DeserializeEntityHierarchy(entities, path);
    for (const ecs::Entity e : entities)
    {
        AddEntityToSceneView(e);
    }
    return entities[0];
}

}