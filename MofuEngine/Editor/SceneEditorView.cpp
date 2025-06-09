#include "SceneEditorView.h"
#include "ECS/Scene.h"
#include "imgui.h"
#include "ECS/Component.h"

namespace mofu::editor {
namespace {

// TODO: adjust this
struct EntityTreeNode
{
    char Name[28] = "";
    ecs::Entity ID;
    EntityTreeNode* Parent = NULL;
    ImVector<EntityTreeNode*> Childs;
    u16              IndexInParent = 0;
};

// TODO: can probably do it better
static EntityTreeNode* CreateSceneHierarchyTree(const Vec<ecs::EntityData>& entityData)
{
    EntityTreeNode* root = new EntityTreeNode{};
	char rootName[28]{ "Root" };
    snprintf(root->Name, IM_ARRAYSIZE(root->Name), "%s", rootName);
    root->ID = ecs::Entity{ (id_t) -1};

	for (const ecs::EntityData& e : entityData)
	{
		log::Info("found entity: %u, block: %p, row: %u", e.id, e.block, e.row);

        EntityTreeNode* node = new EntityTreeNode{};
		node->ID = e.id;
        snprintf(node->Name, IM_ARRAYSIZE(node->Name), "E %u", (u32)e.id);
		node->Parent = root;
		node->IndexInParent = (u16)root->Childs.Size;
		root->Childs.push_back(node);
	}

    return root;
}

struct SceneHierarchy
{
    ImGuiTextFilter Filter;
    EntityTreeNode* VisibleNode = NULL;

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

                EntityData& entityData{ ecs::scene::GetEntityData(node->ID) };
                EntityBlock* block{ ecs::scene::GetEntityData(node->ID).block };
                u32 currentOffset{ sizeof(Entity) };
                for (ComponentID cid{ 0 }; cid < component::ComponentTypeCount; ++cid)
                {
                    if (!block->Signature.test(cid)) continue;
                    block->ComponentOffsets[cid] = currentOffset;
                    void* componentArray{ block->ComponentData + currentOffset };
                    component::RenderLUT[cid](static_cast<u8*>(componentArray) + entityData.row * component::GetComponentSize(cid));

                    currentOffset += component::GetComponentSize(cid) * MAX_ENTITIES_PER_BLOCK;
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
        bool node_open = ImGui::TreeNodeEx("", tree_flags, "%s", node->Name);
        if (ImGui::IsItemFocused())
            VisibleNode = node;
        if (node_open)
        {
            for (EntityTreeNode* child : node->Childs)
                DrawTreeNode(child);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
};

SceneHierarchy sceneHierarchy;
EntityTreeNode* sceneTree;

}


bool 
InitializeSceneEditorView()
{

    const Vec<ecs::EntityData>& entityData = ecs::scene::GetAllEntityData();
    for (const ecs::EntityData e : entityData)
    {
        log::Info("found entity: %u, block: %p, row: %u", e.id, e.block, e.row);
    }

    sceneTree = CreateSceneHierarchyTree(entityData);

    return true;
}

void RenderSceneEditorView()
{
    ImGui::Begin("Scene hierarchy");

    sceneHierarchy.Draw(sceneTree);

    ImGui::End();
}
}