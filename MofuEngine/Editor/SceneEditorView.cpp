#include "SceneEditorView.h"
#include "imgui.h"
#include "ECS/Component.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "MaterialEditor.h"

namespace mofu::editor {
namespace {

// TODO: adjust this
struct EntityTreeNode
{
    char Name[28] = "";
    ecs::Entity ID;
    EntityTreeNode* Parent = NULL;
    ImVector<EntityTreeNode*> Childs;
    u16 IndexInParent = 0;
};

EntityTreeNode* rootNode{ nullptr };
Vec<std::pair<ecs::Entity, EntityTreeNode*>> entityToPair{};

constexpr EntityTreeNode*
FindParentAsNode(ecs::Entity parentEntity)
{
    for (auto [e, n] : entityToPair)
    {
        if (e == parentEntity) return n;
    }
    assert(false);
   //TODO: return std::find;
}

void
CreateEntityTreeNode(ecs::Entity entity, EntityTreeNode* parentNode)
{
    if (!parentNode) parentNode = rootNode;
    EntityTreeNode* node = new EntityTreeNode{};
    node->ID = entity;
    snprintf(node->Name, IM_ARRAYSIZE(node->Name), "E %u", (u32)entity);
    node->Parent = parentNode;
    node->IndexInParent = (u16)parentNode->Childs.Size;
    parentNode->Childs.push_back(node);
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

void CreateEntity(EntityTreeNode* node)
{
	ecs::Entity selected{ node ? node->ID : id::INVALID_ID };

    ecs::component::LocalTransform lt{};
    ecs::component::WorldTransform wt{};

    //TODO: cleaner
    ecs::EntityData entityData = id::IsValid(selected)
        ? ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform, 
            ecs::component::Parent>(lt, wt, ecs::component::Parent{ {} })
        : ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform>(lt, wt);

	log::Info("Created entity %u", entityData.id);
	if (id::IsValid(selected)) log::Info("Entity has parent: %u", selected);

	CreateEntityTreeNode(entityData.id, node);
}

struct SceneHierarchy
{
    ImGuiTextFilter Filter;
    EntityTreeNode* VisibleNode{ nullptr };

    void Draw(EntityTreeNode* root_node)
    {
        // Left side: draw tree
        if (ImGui::BeginChild("##tree", ImVec2(300, 0), ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened))
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
                for (EntityTreeNode* node : root_node->Childs)
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
            ImGui::TextDisabled("UID: 0x%08X", node->ID);
            ImGui::Separator();

            if (ImGui::BeginTable("##properties", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
            {
                ImGui::PushID((int)node->ID);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 2.0f); // Default twice larger

                //ecs::component::RenderComponentFields<ecs::component::LocalTransform>(
                //    ecs::scene::GetEntityComponent<ecs::component::LocalTransform>(node->ID));

                using namespace ecs;

                //TODO: make an iterator or a view
                EntityData& entityData{ ecs::scene::GetEntityData(node->ID) };
                EntityBlock* block{ ecs::scene::GetEntityData(node->ID).block };
                u32 currentOffset{ sizeof(Entity) * MAX_ENTITIES_PER_BLOCK };
                for (ComponentID cid{ 0 }; cid < component::ComponentTypeCount; ++cid)
                {
                    if (!block->Signature.test(cid)) continue;
                    block->ComponentOffsets[cid] = currentOffset;
                    void* componentArray{ block->ComponentData + currentOffset };
                    component::RenderLUT[cid](static_cast<u8*>(componentArray) + entityData.row * component::GetComponentSize(cid));

                    currentOffset += component::GetComponentSize(cid) * MAX_ENTITIES_PER_BLOCK;
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::Button("+ Add Component"))
                    ImGui::OpenPopup("AddComponentPopup");

                if (ImGui::BeginPopup("AddComponentPopup"))
                {
                    for (ecs::ComponentID cid{ 0 }; cid < ecs::component::ComponentTypeCount; ++cid)
                    {
                        if (block->Signature.test(cid)) continue;

                        if (ImGui::Selectable(ecs::component::ComponentNames[cid]))
                        {
                            ecs::component::AddLUT[cid](node->ID);
                            block->Signature.set(cid);
                        }
                    }
                    ImGui::EndPopup();
                }

                if (ecs::scene::HasComponent<ecs::component::RenderMaterial>(entityData.id))
                {
                    if (ImGui::Button("Edit Material"))
                    {
                        ecs::component::RenderMaterial mat{ ecs::scene::GetComponent<ecs::component::RenderMaterial>(entityData.id) };
                        material::OpenMaterialEditor(entityData.id, mat);
                    }
                }
                else
                {
                    if (ImGui::Button("Add Material"))
                    {
                        material::OpenMaterialCreator(entityData.id);
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
        if (node->Childs.Size == 0)
            tree_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
        bool node_open{ ImGui::TreeNodeEx("", tree_flags, "%s", node->Name) };
        if (ImGui::IsItemFocused())
            VisibleNode = node;
        if (node_open)
        {
            for (EntityTreeNode* child : node->Childs)
                DrawTreeNode(child);
            ImGui::TreePop();
        }
        if (ImGui::BeginPopupContextItem("NodeContextMenu"))
        {
            if (ImGui::MenuItem("Create Child Entity"))
                CreateEntity(node);
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