#include "SceneEditorView.h"
#include "imgui.h"
#include "ECS/Component.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "MaterialEditor.h"
#include "AssetInteraction.h"
#include "Project/Project.h"

namespace mofu::editor {
namespace {

// TODO: adjust this
struct EntityTreeNode
{
    char Name[28] = "";
    ecs::Entity ID;
    EntityTreeNode* Parent = NULL;
    ImVector<EntityTreeNode*> Children;
    u16 IndexInParent = 0;
};

EntityTreeNode* rootNode{ nullptr };
Vec<std::pair<ecs::Entity, EntityTreeNode*>> entityToPair{};

Vec<std::string> prefabFiles{};

constexpr EntityTreeNode*
FindParentAsNode(ecs::Entity parentEntity)
{
    for (auto [e, n] : entityToPair)
    {
        if (e == parentEntity) return n;
    }
    assert(false);
   //TODO: return std::find;
    return nullptr;
}

void
CreateEntityTreeNode(ecs::Entity entity, EntityTreeNode* parentNode)
{
    if (!parentNode) parentNode = rootNode;
    EntityTreeNode* node = new EntityTreeNode{};
    node->ID = entity;
    if (ecs::scene::HasComponent<ecs::component::NameComponent>(entity))
    {
        const char* name{ ecs::scene::GetComponent<ecs::component::NameComponent>(entity).Name };
        strcpy(node->Name, name);
    }
    else
    {
       snprintf(node->Name, IM_ARRAYSIZE(node->Name), "E %u", (u32)entity);
    }
    node->Parent = parentNode;
    node->IndexInParent = (u16)parentNode->Children.Size;
    parentNode->Children.push_back(node);
    entityToPair.emplace_back(entity, node);
}

// TODO: can probably do it better
static void CreateSceneHierarchyTree(const Vec<ecs::EntityData>& entityData)
{
    rootNode = new EntityTreeNode{};
	char rootName[28]{ "Root" };
    snprintf(rootNode->Name, IM_ARRAYSIZE(rootNode->Name), "%s", rootName);
    rootNode->ID = ecs::Entity{ id::INVALID_ID };

    for (const ecs::EntityData& e : entityData)
    {
        CreateEntityTreeNode(e.id, rootNode);
	}
}

void DeleteEntity(EntityTreeNode* node)
{
    if (!node) return;
    ecs::Entity selected{ node->ID };
    log::Error("Not Implemented");
}

void 
CreateEntity(EntityTreeNode* node)
{
	ecs::Entity selected{ node ? node->ID : id::INVALID_ID };

    ecs::component::LocalTransform lt{};

    //TODO: cleaner
    const ecs::EntityData& entityData = id::IsValid(selected)
        ? ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::Child>(lt, ecs::component::Child{ {}, selected })
        : ecs::scene::SpawnEntity<ecs::component::LocalTransform>(lt);

	CreateEntityTreeNode(entityData.id, node);
}

void 
DuplicateEntity(EntityTreeNode* node)
{
    if (!node) return;
    ecs::Entity selected{ node->ID };

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
        mesh.RenderItemID = graphics::AddRenderItem(newEntity, mesh.MeshID, material.MaterialCount, material.MaterialIDs);
    }

    EntityTreeNode* parentNode{ rootNode };
    if (ecs::scene::HasComponent<ecs::component::Child>(newEntity))
    {
        ecs::component::Child child{ ecs::scene::GetComponent<ecs::component::Child>(newEntity) };
        parentNode = FindParentAsNode(child.ParentEntity);
    }
    CreateEntityTreeNode(newEntity, node);
}

struct SceneHierarchy
{
    ImGuiTextFilter Filter;
    EntityTreeNode* VisibleNode{ nullptr };

    void Draw(EntityTreeNode* root_node)
    {
        ImGui::BeginGroup();
        if (ImGui::Button("Import Hierarchy"))
        {
            prefabFiles.clear();
            content::ListFilesByExtensionRec(".pre", project::GetResourceDirectory() / "Prefabs", prefabFiles);
            ImGui::OpenPopup("ImportHierarchyPopup");
        }
        if (ImGui::BeginPopup("ImportHierarchyPopup"))
        {
            for (std::string_view path : prefabFiles)
            {
                if (ImGui::Selectable(path.data()))
                {
                    Vec<ecs::Entity> entities{};
                    assets::DeserializeEntityHierarchy(entities, path);
                    for (const ecs::Entity e : entities)
                    {
                        AddEntityToSceneView(e);
                    }
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
        if (ImGui::Button("Add Entity"))
        {
            ecs::component::LocalTransform transform{};
            ecs::EntityData& entityData{ ecs::scene::SpawnEntity<ecs::component::LocalTransform>(transform) };
            AddEntityToSceneView(entityData.id);
        }
        ImGui::EndGroup();

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
                for (EntityTreeNode* node : root_node->Children)
                    if (Filter.PassFilter(node->Name)) // Filter root node
                        DrawTreeNode(node);
                ImGui::EndTable();
            }

            if (ImGui::BeginPopupContextWindow("SceneHierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
            {
                if (ImGui::MenuItem("Create Entity"))
                    CreateEntity(VisibleNode);
                ImGui::EndPopup();
            }
        }
        ImGui::EndChild();

        // Right side: draw properties
        ImGui::SameLine();

        ImGui::BeginGroup(); // Lock X position
        if (EntityTreeNode* node = VisibleNode)
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
        if (node == VisibleNode)
            tree_flags |= ImGuiTreeNodeFlags_Selected;
        if (node->Children.Size == 0)
            tree_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
        bool node_open{ ImGui::TreeNodeEx("", tree_flags, "%s", node->Name) };
        if (ImGui::IsItemFocused())
            VisibleNode = node;
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

}


//TODO: instead of adding 1 by 1 add a whole bunch from tables of to add and to remove or sth?
void AddEntityToSceneView(ecs::Entity entity)
{
    assert(ecs::scene::IsEntityAlive(entity));

    EntityTreeNode* parentNode{ rootNode };
    if (ecs::scene::HasComponent<ecs::component::Child>(entity))
    {
        ecs::component::Child p{ ecs::scene::GetComponent<ecs::component::Child>(entity) };
        parentNode = FindParentAsNode(p.ParentEntity);
    }

    CreateEntityTreeNode(entity, parentNode);
}

bool 
InitializeSceneEditorView()
{
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

    sceneHierarchy.Draw(rootNode);

    ImGui::End();
}
}