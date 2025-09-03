#include "ContentBrowser.h"
#include "imgui.h"
#include <filesystem>
#include "Utilities/Logger.h"
#include "Content/AssetImporter.h"
#include "Content/PrimitiveMeshGeneration.h"
#include "AssetInteraction.h"
#include "Content/TextureImport.h"
#include "TextureView.h"
#include "ImporterView.h"
#include "Content/Asset.h"
#include "Content/EditorContentManager.h"
#include "Editor/Project/Project.h"
#include "EditorInterface.h"

namespace mofu::editor {

namespace {
std::filesystem::path assetBaseDirectory;
std::filesystem::path assetImportedDirectory;
std::filesystem::path currentDirectory;
std::filesystem::path projectBaseDirectory;

std::bitset<content::AssetType::Count> itemTypeFilter{};

struct Selection : ImGuiSelectionBasicStorage
{
    // Find which item should be Focused after deletion.
    // Call _before_ item submission. Return an index in the before-deletion item list, your item loop should call SetKeyboardFocusHere() on it.
    // The subsequent ApplyDeletionPostLoop() code will use it to apply Selection.
    // - We cannot provide this logic in core Dear ImGui because we don't have access to selection data.
    // - We don't actually manipulate the ImVector<> here, only in ApplyDeletionPostLoop(), but using similar API for consistency and flexibility.
    // - Important: Deletion only works if the underlying ImGuiID for your items are stable: aka not depend on their index, but on e.g. item id/ptr.
    // FIXME-MULTISELECT: Doesn't take account of the possibility focus target will be moved during deletion. Need refocus or scroll offset.
    int ApplyDeletionPreLoop(ImGuiMultiSelectIO* multiselectIO, int items_count)
    {
        if (Size == 0)
            return -1;

        // If focused item is not selected...
        const int focused_idx = (int)multiselectIO->NavIdItem;  // Index of currently focused item
        if (multiselectIO->NavIdSelected == false)  // This is merely a shortcut, == Contains(adapter->IndexToStorage(items, focused_idx))
        {
            multiselectIO->RangeSrcReset = true;    // Request to recover RangeSrc from NavId next frame. Would be ok to reset even when NavIdSelected==true, but it would take an extra frame to recover RangeSrc when deleting a selected item.
            return focused_idx;             // Request to focus same item after deletion.
        }

        // If focused item is selected: land on first unselected item after focused item.
        for (int idx = focused_idx + 1; idx < items_count; idx++)
            if (!Contains(GetStorageIdFromIndex(idx)))
                return idx;

        // If focused item is selected: otherwise return last unselected item before focused item.
        for (int idx = std::min(focused_idx, items_count) - 1; idx >= 0; idx--)
            if (!Contains(GetStorageIdFromIndex(idx)))
                return idx;

        return -1;
    }

    // Rewrite item list (delete items) + update selection.
    // - Call after EndMultiSelect()
    // - We cannot provide this logic in core Dear ImGui because we don't have access to your items, nor to selection data.
    template<typename ITEM_TYPE>
    void ApplyDeletionPostLoop(ImGuiMultiSelectIO* multiselectIO, ImVector<ITEM_TYPE>& items, int item_curr_idx_to_select)
    {
        // Rewrite item list (delete items) + convert old selection index (before deletion) to new selection index (after selection).
        // If NavId was not part of selection, we will stay on same item.
        ImVector<ITEM_TYPE> new_items;
        new_items.reserve(items.Size - Size);
        int item_next_idx_to_select = -1;
        for (int idx = 0; idx < items.Size; idx++)
        {
            if (!Contains(GetStorageIdFromIndex(idx)))
                new_items.push_back(items[idx]);
            if (item_curr_idx_to_select == idx)
                item_next_idx_to_select = new_items.Size - 1;
        }
        items.swap(new_items);

        // Update selection
        Clear();
        if (item_next_idx_to_select != -1 && multiselectIO->NavIdSelected)
            SetItemSelected(GetStorageIdFromIndex(item_next_idx_to_select), true);
    }
};

struct AssetTreeNode
{
    //TODO: imitates a directory hierarchy? kinda a B-Tree
    std::filesystem::path FullPath;
    std::string Name;
    AssetTreeNode* Parent;
    Vec<AssetTreeNode*> Children;
    Vec<std::pair<content::AssetHandle, content::AssetPtr>> Assets;
};

struct AssetBrowser
{
    // Options
    bool ShowTypeOverlay = true;
    bool AllowSorting = true;
    bool AllowDragUnselected = false;
    bool AllowBoxSelect = true;
    f32 IconSize = 80.0f;
    int  IconSpacing = 24;
    int  IconHitSpacing = 24;
    bool StretchSpacing = true;

    // State
    //ImVector<std::pair<content::AssetHandle, content::AssetPtr>> Items;
    Vec<std::unique_ptr<AssetTreeNode>> AssetTree{};
    AssetTreeNode* CurrentNode{};
	AssetTreeNode* GetRootNode() const { return AssetTree[0].get(); }
    Selection Selection;
    std::set<content::AssetHandle> Selected{};
    bool RequestDelete = false; // deferred deletion request
    bool RequestSort = false; // deferred sort request
    f32 ZoomWheelAccum = 0.0f; // Mouse wheel accumulator to handle smooth wheels better

    // Calculated sizes for layout, output of UpdateLayoutSizes(). Could be locals but our code is simpler this way.
    ImVec2 LayoutItemSize;
    ImVec2 LayoutItemStep; // == LayoutItemSize + LayoutItemSpacing
    f32 LayoutItemSpacing = 0.0f;
    f32 LayoutSelectableSpacing = 0.0f;
    f32 LayoutOuterPadding = 0.0f;
    u32 LayoutColumnCount = 0;
    u32 LayoutLineCount = 0;

    AssetBrowser()
    {
        
    }

    void CreateRoot()
    {
        auto root{ std::make_unique<AssetTreeNode>(assetImportedDirectory, assetImportedDirectory.filename().string(), nullptr) };
        CurrentNode = root.get();
        AssetTree.emplace_back(std::move(root));
    }

    AssetTreeNode* 
    FindOrCreateNodeForPath(const std::filesystem::path& assetPath) 
    {
        std::filesystem::path relativePath = std::filesystem::relative(assetPath, assetImportedDirectory);

        AssetTreeNode* currentNode{ GetRootNode() };

        // iterate through each path component until the filename
        for (const auto& component : relativePath.parent_path())
        {
            // skip empty components
            if (component.empty() || component == ".") continue;

            // Check if this component already exists as a child
            AssetTreeNode* componentAsChild = nullptr;
            for (auto& child : currentNode->Children) 
            {
                if (child->Name == component.string()) 
                {
                    componentAsChild = child;
                    break;
                }
            }

            // If not found, create a new node
            if (!componentAsChild) 
            {
                auto node{ std::make_unique<AssetTreeNode>(currentNode->FullPath / component, component.string(), currentNode) };
                componentAsChild = node.get();
                AssetTree.emplace_back(std::move(node));

                currentNode->Children.emplace_back(componentAsChild);
            }

            // Move to this child for next iteration
            currentNode = componentAsChild;
        }

        return currentNode;
    }

    void AddItem(content::AssetHandle handle) 
    {
        content::AssetPtr asset{ content::assets::GetAsset(handle) };
        const auto& assetPath{ asset->ImportedFilePath };
        AssetTreeNode* parentDirectory{ FindOrCreateNodeForPath(assetPath) };
        parentDirectory->Assets.emplace_back(handle, asset);
        RequestSort = true;
    }

    void AddItem(content::AssetHandle handle, content::AssetPtr asset)
    {
        const auto& assetPath{ asset->ImportedFilePath };
        AssetTreeNode* parentDirectory{ FindOrCreateNodeForPath(assetPath) };
        parentDirectory->Assets.emplace_back(handle, asset);
        RequestSort = true;
    }

    // Logic would be written in the main code BeginChild() and outputting to local variables.
    // We extracted it into a function so we can call it easily from multiple places.
    void UpdateLayoutSizes(const AssetTreeNode* currentNode, f32 availWidth)
    {
        // Layout: when not stretching: allow extending into right-most spacing.
        LayoutItemSpacing = (float)IconSpacing;
        if (StretchSpacing == false)
            availWidth += floorf(LayoutItemSpacing * 0.5f);

        // Layout: calculate number of icon per line and number of lines
        LayoutItemSize = ImVec2(floorf(IconSize), floorf(IconSize));
        LayoutColumnCount = std::max((int)(availWidth / (LayoutItemSize.x + LayoutItemSpacing)), 1);
        LayoutLineCount = (currentNode->Assets.size() + LayoutColumnCount - 1) / LayoutColumnCount;

        // Layout: when stretching: allocate remaining space to more spacing. Round before division, so item_spacing may be non-integer.
        if (StretchSpacing && LayoutColumnCount > 1)
            LayoutItemSpacing = floorf(availWidth - LayoutItemSize.x * LayoutColumnCount) / LayoutColumnCount;

        LayoutItemStep = ImVec2(LayoutItemSize.x + LayoutItemSpacing, LayoutItemSize.y + LayoutItemSpacing);
        LayoutSelectableSpacing = std::max(floorf(LayoutItemSpacing) - IconHitSpacing, 0.0f);
        LayoutOuterPadding = floorf(LayoutItemSpacing * 0.5f);
    }

    void Draw(const char* title, bool* pOpen);
};



AssetBrowser assetBrowser;
AssetBrowser fileBrowser;

BrowserMode browserMode{ BrowserMode::Assets };

void
DrawAssetTypeFilter()
{
    if (ImGui::Button("All")) itemTypeFilter.set();
    ImGui::SameLine();
    if (ImGui::Button("None"))  itemTypeFilter.reset();

    for (u32 i{ 0 }; i < content::AssetType::Count; ++i)
    {
        content::AssetType::type type{ (content::AssetType::type)i };
        bool isSelected{ itemTypeFilter.test(i) };

        ImGui::SameLine();
        if (ImGui::Checkbox(content::ASSET_TYPE_TO_STRING[type], &isSelected))
        {
            itemTypeFilter.set(i, isSelected);
        }
    }
}

void
AssetBrowser::Draw(const char* title, bool* pOpen)
{
    // Menu bar
    if (ImGui::BeginMenuBar())
    {
        if (CurrentNode->Parent)
        {
            if (ImGui::Button(".."))
            {
                CurrentNode = CurrentNode->Parent;
            }
        }
        if (ImGui::MenuItem("Mode"))
        {
            browserMode = browserMode == BrowserMode::Assets ? BrowserMode::AllFiles : BrowserMode::Assets;
        }
        if (ImGui::MenuItem("Clear Selection"))
        {
            Selected.clear();
        }
        DrawAssetTypeFilter();

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Generate items"))
            {
                for (u32 i{ 0 }; i < 5; ++i)
                {
                    content::Asset* asset = new content::Asset(content::AssetType::Texture, {}, {"test"});
                    content::AssetHandle handle{ content::assets::RegisterAsset(asset) };
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Close", NULL, false, pOpen != NULL))
                *pOpen = false;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Delete", "Del", false, Selection.Size > 0))
                RequestDelete = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::PushItemWidth(ImGui::GetFontSize() * 10);

            ImGui::SeparatorText("Contents");
            ImGui::Checkbox("Show Type Overlay", &ShowTypeOverlay);
            ImGui::Checkbox("Allow Sorting", &AllowSorting);

            ImGui::SeparatorText("Selection Behavior");
            ImGui::Checkbox("Allow dragging unselected item", &AllowDragUnselected);
            ImGui::Checkbox("Allow box-selection", &AllowBoxSelect);

            ImGui::SeparatorText("Layout");
            ImGui::SliderFloat("Icon Size", &IconSize, 16.0f, 128.0f, "%.0f");
            ImGui::SliderInt("Icon Spacing", &IconSpacing, 0, 32);
            ImGui::SliderInt("Icon Hit Spacing", &IconHitSpacing, 0, 32);
            ImGui::Checkbox("Stretch Spacing", &StretchSpacing);
            ImGui::PopItemWidth();
            ImGui::EndMenu();
        }
        if (ImGui::Button("Save Assets"))
        {
            content::assets::SaveAssetRegistry();
        }
        ImGui::EndMenuBar();
    }

    ImGui::SetNextWindowContentSize(ImVec2(0.0f, LayoutOuterPadding + LayoutLineCount * (LayoutItemSize.y + LayoutItemSpacing)));
    if (ImGui::BeginChild("Assets", ImVec2(0.0f, -ImGui::GetTextLineHeightWithSpacing()), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoMove))
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const float availWidth = ImGui::GetContentRegionAvail().x;
        UpdateLayoutSizes(CurrentNode, availWidth);

        // Calculate and store start position
        ImVec2 startPos = ImGui::GetCursorScreenPos();
        startPos = ImVec2(startPos.x + LayoutOuterPadding, startPos.y + LayoutOuterPadding);
        ImGui::SetCursorScreenPos(startPos);
        ImGui::Dummy(ImVec2(0.f, 0.f));

        if (itemTypeFilter == 0) ImGui::TextUnformatted("Select at least one asset type");

        // Multi-select
        ImGuiMultiSelectFlags multiselectFlags = ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid | ImGuiMultiSelectFlags_NavWrapX;
        if (AllowBoxSelect)
            multiselectFlags |= ImGuiMultiSelectFlags_BoxSelect2d;
        if (AllowDragUnselected)
            multiselectFlags |= ImGuiMultiSelectFlags_SelectOnClickRelease;

        auto& assets{ CurrentNode->Assets };
        u32 assetCount{ (u32)assets.size() };
        ImGuiMultiSelectIO* multiselectIO = ImGui::BeginMultiSelect(multiselectFlags, Selection.Size, assetCount);

        // Use custom selection adapter: store ID in selection
        Selection.UserData = this;
        Selection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self_, int idx) {
            AssetBrowser* self = (AssetBrowser*)self_->UserData; return ImGui::GetID(self->CurrentNode->Assets[idx].first.id); };
        Selection.ApplyRequests(multiselectIO);

        const bool deleteRequested = (ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_Repeat) && (Selection.Size > 0)) || RequestDelete;
        const int itemCurrentIdxToFocus = deleteRequested ? Selection.ApplyDeletionPreLoop(multiselectIO, assetCount) : -1;
        RequestDelete = false;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(LayoutSelectableSpacing, LayoutSelectableSpacing));

        // Rendering parameters
        const ImU32 iconTypeOverlayColors[content::AssetType::Count] = { 0, IM_COL32(200, 70, 70, 255), IM_COL32(70, 170, 70, 255), 
            IM_COL32(5, 70, 200, 255), IM_COL32(200, 200, 230, 255), IM_COL32(10, 190, 140, 255) };
        const ImU32 iconBgColor = ImGui::GetColorU32(IM_COL32(35, 35, 35, 220));
        const ImU32 nodeBgColor = ImGui::GetColorU32(IM_COL32(35, 35, 45, 220));
        const ImVec2 iconTypeOverlaySize = ImVec2(4.0f, 4.0f);
        const bool displayLabel = (LayoutItemSize.x >= ImGui::CalcTextSize("999").x);

        const u32 columnCount = LayoutColumnCount;
        s32 drawnItemCount{ 0 };
        s32 discardedItemCount{ 0 };
        ImGuiListClipper clipper;
        clipper.Begin(LayoutLineCount, LayoutItemStep.y);
        if (itemCurrentIdxToFocus != -1)
            clipper.IncludeItemByIndex(itemCurrentIdxToFocus / columnCount);
        if (multiselectIO->RangeSrcItem != -1)
            clipper.IncludeItemByIndex((int)multiselectIO->RangeSrcItem / columnCount);

        Vec<AssetTreeNode*>& currentChildren{ CurrentNode->Children };
        u32 childrenNodesCount{ (u32)currentChildren.size() };
		bool changedNode{ false };
        //for (auto node : CurrentNode->Children)
        //{
        //    if (ImGui::Button(node->Name.c_str()))
        //    {
        //        CurrentNode = node;
        //    }
        //}

        while (clipper.Step() && !changedNode)
        {
            for (int lineIdx = clipper.DisplayStart; lineIdx < clipper.DisplayEnd && !changedNode; ++lineIdx)
            {
                const u32 minCurrLineIdx = lineIdx * columnCount + discardedItemCount;
                int maxCurrLineIdx = std::min(minCurrLineIdx + columnCount, assetCount);
                for (int itemIdx = minCurrLineIdx; itemIdx < maxCurrLineIdx; ++itemIdx)
                {
                    if (itemIdx < childrenNodesCount)
                    {
						auto node{ currentChildren[itemIdx] };

                        ImVec2 pos = ImVec2(startPos.x + (drawnItemCount % columnCount) * LayoutItemStep.x, startPos.y + lineIdx * LayoutItemStep.y);
                        ImGui::SetCursorScreenPos(pos);
                        ImGui::PushID(itemIdx);
                        bool itemIsSelected = Selection.Contains(itemIdx);

                        ImVec2 boxMin(pos.x - 1, pos.y - 1);
                        ImVec2 boxMax(boxMin.x + LayoutItemSize.x + 2, boxMin.y + LayoutItemSize.y + 2);
                        ImGui::Selectable("", itemIsSelected, ImGuiSelectableFlags_None, LayoutItemSize);
                        drawList->AddRectFilled(boxMin, boxMax, nodeBgColor);
                        drawnItemCount++;

                        if (ImGui::IsItemHovered())
                        {
                            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                            {
                                CurrentNode = node;
                                changedNode = true;
                            }
                        }

                        ImU32 labelColor = ImGui::GetColorU32(itemIsSelected ? ImGuiCol_Text : ImGuiCol_TextDisabled);
                        s32 textOffset{ drawnItemCount % 2 ? 16 : 0 };
                        drawList->AddText(ImVec2(boxMin.x, boxMax.y - ImGui::GetFontSize() + textOffset), labelColor, node->Name.c_str());

                        ImGui::PopID();
                        continue;
                    }

                    auto& item = assets[itemIdx];
                    if (!itemTypeFilter.test(item.second->Type))
                    {
                        maxCurrLineIdx++;
                        if (maxCurrLineIdx >= assetCount)
                        {
                            lineIdx = clipper.DisplayEnd;
                            break;
                        }
                        discardedItemCount++;
                        continue;
                    }

                    content::AssetHandle handle{ item.first };
                    ImGuiID imGuiID{ ImGui::GetID(handle.id) };
                    ImGui::PushOverrideID(imGuiID);

                    // Position item
                    ImVec2 pos = ImVec2(startPos.x + (drawnItemCount % columnCount) * LayoutItemStep.x, startPos.y + lineIdx * LayoutItemStep.y);
                    ImGui::SetCursorScreenPos(pos);

                    ImGui::SetNextItemSelectionUserData(itemIdx);

                    bool itemIsSelected{ Selected.contains(handle) };
                    
                    //bool itemIsSelected = Selection.Contains(imGuiID);
                    bool itemIsVisible = ImGui::IsRectVisible(LayoutItemSize);
                    if (ImGui::Selectable("", itemIsSelected, ImGuiSelectableFlags_None, LayoutItemSize))
                    {
                        if (itemIsSelected)
                        {
                            Selected.erase(handle);

                            Vec<std::string> files{};
                            for (const content::AssetHandle h : Selected)
                            {
                                content::AssetPtr a{ content::assets::GetAsset(h) };
                                if (a->Type == content::AssetType::Texture)
                                {
                                    files.emplace_back(a->OriginalFilePath.string());
                                }
                            }
                            assets::RefreshFiles(files);
                        }
                        else
                        {
                            Selected.emplace(handle);
                        }
                        itemIsSelected = !itemIsSelected;
                    }

                    // Update our selection state immediately because we use this to alter the color of our text/icon
                    //TODO: use the imgui Multiselection
                   /* if (ImGui::IsItemToggledSelection())
                    {
                        if (!itemIsSelected)
                        {
                            Selected.emplace_back(item.first);
                        }
                        else
                        {
                            Selected.erase_unordered(item.first);
                        }
                        itemIsSelected = !itemIsSelected;
                    }*/


                    // Focus (for after deletion)
                    if (itemCurrentIdxToFocus == itemIdx)
                        ImGui::SetKeyboardFocusHere(-1);

                    // Drag and drop
                    if (ImGui::BeginDragDropSource())
                    {
                        // Create payload with full selection or single unselected item
                        if (ImGui::GetDragDropPayload() == NULL)
                        {
                            ImVector<ImGuiID> payloadItems;
                            void* it = NULL;
                            ImGuiID id = 0;
                            if (!itemIsSelected)
                                payloadItems.push_back(id);
                            else
                                while (Selection.GetNextSelectedItem(&it, &id))
                                    payloadItems.push_back(id);
                            ImGui::SetDragDropPayload("ASSETS_BROWSER_ITEMS", payloadItems.Data, (size_t)payloadItems.size_in_bytes());
                        }

                        // Display payload content in tooltip, by extracting it from the payload data
                        const ImGuiPayload* payload = ImGui::GetDragDropPayload();
                        const int payloadCount = (int)payload->DataSize / (int)sizeof(ImGuiID);
                        ImGui::Text("%d assets", payloadCount);

                        ImGui::EndDragDropSource();
                    }

                    bool itemIsHovered = ImGui::IsItemHovered();

                    // Render asset icon
                    if (itemIsVisible)
                    {
                        ImVec2 boxMin(pos.x - 1, pos.y - 1);
                        ImVec2 boxMax(boxMin.x + LayoutItemSize.x + 2, boxMin.y + LayoutItemSize.y + 2);
                        if (item.second->Type == content::AssetType::Texture)
                        {
                            id_t iconID{ item.second->AdditionalData };
                            if (id::IsValid(iconID))
                            {
                                drawList->AddImage(graphics::ui::GetImTextureID(iconID), boxMin, boxMax);
                            }
                            else
                            {
                                // TODO: placeholder icon
                                drawList->AddRectFilled(boxMin, boxMax, iconBgColor);
                            }
                            if (item.second->RelatedCount != 1)
                            {
                                char label[2];
                                snprintf(label, 2, "%u", item.second->RelatedCount);
                                drawList->AddText(ImVec2(boxMin.x, boxMin.y + ImGui::GetFontSize() + 4), ImGuiCol_TextDisabled, label);
                            }
                        }
                        else
                        {
                            drawList->AddRectFilled(boxMin, boxMax, iconBgColor);
                        }
                        if (ShowTypeOverlay)
                        {
                            ImU32 typeColor = iconTypeOverlayColors[item.second->Type % IM_ARRAYSIZE(iconTypeOverlayColors)];
                            drawList->AddRectFilled(ImVec2(boxMax.x - 2 - iconTypeOverlaySize.x, boxMin.y + 2),
                                ImVec2(boxMax.x - 2, boxMin.y + 2 + iconTypeOverlaySize.y), typeColor);
                        }
                        if (displayLabel)
                        {
                            ImU32 labelColor = ImGui::GetColorU32(itemIsSelected ? ImGuiCol_Text : ImGuiCol_TextDisabled);
                            char label[32];
                            s32 textOffset{ drawnItemCount % 2 ? 16 : 0 };
                            snprintf(label, 32, "%s", item.second->Name.data());
                            bool isImported{ std::filesystem::exists(content::assets::GetAsset(handle)->ImportedFilePath) };
                            drawList->AddText(ImVec2(boxMin.x, boxMax.y - ImGui::GetFontSize() + textOffset), labelColor, label);
                            drawList->AddText(ImVec2(boxMin.x, boxMin.y), labelColor, isImported ? "im" : "nim");
                        }

                        if (itemIsSelected)
                        {
                            if (itemIsHovered)
                            {
                                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                                {
                                    if (item.second->Type == content::AssetType::Texture)
                                    {
                                        //void* it = NULL;
                                        //ImGuiID itemID = 0;
                                        //Vec<ImGuiID> chosenAssets{};
                                        //while (Selection.GetNextSelectedItem(&it, &itemID))
                                        //{
                                        //    chosenAssets.emplace_back(itemID);
                                        //}
                                        /*for (const content::AssetHandle h : Selected)
                                        {
                                            content::AssetPtr a{ content::assets::GetAsset(h) };
                                            if (a->Type == content::AssetType::Texture)
                                            {
                                            }
                                        }*/
                                        assets::AddFile(item.second->OriginalFilePath.string());
                                    }
                                    assets::ViewImportSettings(handle);
                                }
                                if (ImGui::IsKeyPressed(ImGuiKey_C))
                                {
                                    content::ImportAsset(handle);
                                }
                                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                                {
                                    editor::InspectAsset(handle);
                                }
                            }
                        }
                        if (itemIsHovered)
                        {
                            char label[64];
                            snprintf(label, 64, "%s", item.second->Name.data());
                            if(ImGui::BeginTooltip())
                                ImGui::TextUnformatted(label);
                            ImGui::EndTooltip();
                        }
                    }

                    ImGui::PopID();
                    drawnItemCount++;
                }
            }
        }
        clipper.End();
        ImGui::PopStyleVar(); // ImGuiStyleVar_ItemSpacing

        // Context menu
        if (ImGui::BeginPopupContextWindow())
        {
            ImGui::Text("Selection: %d items", Selection.Size);
            ImGui::Separator();
            if (ImGui::MenuItem("Delete", "Del", false, Selection.Size > 0))
                RequestDelete = true;
            ImGui::EndPopup();
        }

        multiselectIO = ImGui::EndMultiSelect();
        Selection.ApplyRequests(multiselectIO);
        if (deleteRequested)
        {
            if (ImGui::BeginPopup("Confirmation")) 
            {
                bool deleteConfirmed = false;

                ImGui::Text("Do you want to delete %u items?", multiselectIO->ItemsCount);
                if (ImGui::Button("Yes")) 
                {
                    deleteConfirmed = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("No")) 
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();

                if (deleteConfirmed) 
                {
                    //Selection.ApplyDeletionPostLoop(multiselectIO, assets, itemCurrentIdxToFocus);
                }
            }
        }
    }
    ImGui::EndChild();

    ImGui::Text("Selected: %d/%d items", Selection.Size, CurrentNode->Assets.size());
}

void
RenderAssetBrowser()
{
    assetBrowser.Draw("Asset Browser", nullptr);
}

void
RenderFileBrowser()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::MenuItem("Mode"))
        {
            browserMode = browserMode == BrowserMode::Assets ? BrowserMode::AllFiles : BrowserMode::Assets;
        }
        ImGui::EndMenuBar();
    }

    if (currentDirectory != projectBaseDirectory)
    {
        if (ImGui::Button("<---"))
        {
            currentDirectory = currentDirectory.parent_path();
        }
    }

    for (auto& dirEntry : std::filesystem::directory_iterator{ currentDirectory })
    {
        const auto& path{ dirEntry.path() };
        std::string filename = path.filename().string();
        const char* name = filename.c_str();
        if (dirEntry.is_directory())
        {
            if (ImGui::Button(name))
            {
                currentDirectory = path;
            }
        }
        else
        {
            std::string extension{ path.extension().string() };
            if (content::IsEngineAssetExtension(extension)) continue;

            //TODO:
            ImGui::PushID(path.string().c_str());

            bool selected = fileBrowser.Selection.Contains((ImGuiID)name);
            bool clicked = ImGui::Selectable(name, selected);

            if (ImGui::IsItemHovered())
            {
                if (content::IsAllowedAssetExtension(extension))
                {
                    content::AssetHandle handle{ content::assets::GetHandleFromPath(path) };
                    assert(handle != content::INVALID_HANDLE);
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        //content::AssetPtr asset{};
                        //if (asset->Type == content::AssetType::Texture)
                        //{
                        //    void* it = NULL;
                        //    ImGuiID id = 0;
                        //    Vec<content::AssetPtr> chosenAssets{};
                        //    while (Selection.GetNextSelectedItem(&it, &id))
                        //    {
                        //        chosenAssets.emplace_back(assets[id].second);
                        //    }
                        //    content::texture::TextureImportSettings settings{};
                        //    settings.FileCount = chosenAssets.size();
                        //    for (const content::AssetPtr a : chosenAssets)
                        //    {
                        //        settings.Files += a->ImportedFilePath.string() + ";";
                        //    }
                        //    assets::ViewImportSettings(id, settings);
                        //}
                        //else
                        {
                            assets::ViewImportSettings(handle);
                        }
                    }
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        //FIXME: temporary
                        content::texture::TextureImportSettings& settings{ assets::GetTextureImportSettingsA() };
                        settings.FileCount = 1;
                        settings.Files = path.string();
                        content::ImportAsset(handle);
                    }
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                    {
                        editor::InspectAsset(handle);
                    }
                }
            }

            ImGui::PopID();
        }
    }
}

void
RenderContentBrowser()
{
    ImGui::Begin("Content Browser", nullptr, ImGuiWindowFlags_MenuBar);

    if (browserMode == BrowserMode::Assets)
    {
        RenderAssetBrowser();
    }
    else
    {
        RenderFileBrowser();
    }

    ImGui::End();
}

} // anonymous namespace

bool
InitializeAssetBrowserGUI()
{
    projectBaseDirectory = editor::project::GetProjectDirectory();
    assetBaseDirectory = editor::project::GetAssetDirectory();
    assetImportedDirectory = editor::project::GetResourceDirectory();
    currentDirectory = projectBaseDirectory;
    assert(std::filesystem::exists(projectBaseDirectory) && std::filesystem::exists(assetBaseDirectory)
        && std::filesystem::exists(assetImportedDirectory) && std::filesystem::exists(currentDirectory));
    itemTypeFilter.set();

    assetBrowser.CreateRoot();

    return true;
}

void 
RenderAssetBrowserGUI()
{
    RenderContentBrowser();
}

void
AddRegisteredAsset(content::AssetHandle handle, content::AssetPtr asset)
{
    content::assets::ParseMetadata(asset);

    assetBrowser.AddItem(handle);
}

void
DeleteRegisteredAsset(content::AssetHandle handle)
{
    //TODO:
}

}