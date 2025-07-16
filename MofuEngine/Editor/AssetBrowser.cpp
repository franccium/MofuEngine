#include "AssetBrowser.h"
#include "imgui.h"
#include <filesystem>
#include "Utilities/Logger.h"
#include "Content/AssetImporter.h"
#include "Content/PrimitiveMeshGeneration.h"
#include "AssetInteraction.h"
#include "Content/TextureImport.h"
#include "TextureView.h"
#include "ImporterView.h"
#include "Editor/Project/Project.h"

namespace mofu::editor {

namespace {
    
struct MyItem
{
    int         ID;
    const char* Name;
    int         Quantity;

};

struct ExampleAsset
{
    ImGuiID ID;
    int     Type;

    ExampleAsset(ImGuiID id, int type) { ID = id; Type = type; }
};

// Extra functions to add deletion support to ImGuiSelectionBasicStorage
struct ExampleSelectionWithDeletion : ImGuiSelectionBasicStorage
{
    // Find which item should be Focused after deletion.
    // Call _before_ item submission. Return an index in the before-deletion item list, your item loop should call SetKeyboardFocusHere() on it.
    // The subsequent ApplyDeletionPostLoop() code will use it to apply Selection.
    // - We cannot provide this logic in core Dear ImGui because we don't have access to selection data.
    // - We don't actually manipulate the ImVector<> here, only in ApplyDeletionPostLoop(), but using similar API for consistency and flexibility.
    // - Important: Deletion only works if the underlying ImGuiID for your items are stable: aka not depend on their index, but on e.g. item id/ptr.
    // FIXME-MULTISELECT: Doesn't take account of the possibility focus target will be moved during deletion. Need refocus or scroll offset.
    int ApplyDeletionPreLoop(ImGuiMultiSelectIO* ms_io, int items_count)
    {
        if (Size == 0)
            return -1;

        // If focused item is not selected...
        const int focused_idx = (int)ms_io->NavIdItem;  // Index of currently focused item
        if (ms_io->NavIdSelected == false)  // This is merely a shortcut, == Contains(adapter->IndexToStorage(items, focused_idx))
        {
            ms_io->RangeSrcReset = true;    // Request to recover RangeSrc from NavId next frame. Would be ok to reset even when NavIdSelected==true, but it would take an extra frame to recover RangeSrc when deleting a selected item.
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
    void ApplyDeletionPostLoop(ImGuiMultiSelectIO* ms_io, ImVector<ITEM_TYPE>& items, int item_curr_idx_to_select)
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
        if (item_next_idx_to_select != -1 && ms_io->NavIdSelected)
            SetItemSelected(GetStorageIdFromIndex(item_next_idx_to_select), true);
    }
};

struct ExampleAssetsBrowser
{
    // Options
    bool            ShowTypeOverlay = true;
    bool            AllowSorting = true;
    bool            AllowDragUnselected = false;
    bool            AllowBoxSelect = true;
    float           IconSize = 32.0f;
    int             IconSpacing = 10;
    int             IconHitSpacing = 4;         // Increase hit-spacing if you want to make it possible to clear or box-select from gaps. Some spacing is required to able to amend with Shift+box-select. Value is small in Explorer.
    bool            StretchSpacing = true;

    // State
    ImVector<ExampleAsset> Items;               // Our items
    ExampleSelectionWithDeletion Selection;     // Our selection (ImGuiSelectionBasicStorage + helper funcs to handle deletion)
    ImGuiID         NextItemId = 0;             // Unique identifier when creating new items
    bool            RequestDelete = false;      // Deferred deletion request
    bool            RequestSort = false;        // Deferred sort request
    float           ZoomWheelAccum = 0.0f;      // Mouse wheel accumulator to handle smooth wheels better

    // Calculated sizes for layout, output of UpdateLayoutSizes(). Could be locals but our code is simpler this way.
    ImVec2          LayoutItemSize;
    ImVec2          LayoutItemStep;             // == LayoutItemSize + LayoutItemSpacing
    float           LayoutItemSpacing = 0.0f;
    float           LayoutSelectableSpacing = 0.0f;
    float           LayoutOuterPadding = 0.0f;
    int             LayoutColumnCount = 0;
    int             LayoutLineCount = 0;

    // Functions
    ExampleAssetsBrowser()
    {
        AddItems(10000);
    }
    void AddItems(int count)
    {
        if (Items.Size == 0)
            NextItemId = 0;
        Items.resize(Items.Size + count);
        for (int n = 0; n < count; n++, NextItemId++)
            Items[n] = (ExampleAsset(NextItemId, (NextItemId % 20) < 15 ? 0 : (NextItemId % 20) < 18 ? 1 : 2));
        RequestSort = true;
    }
    void ClearItems()
    {
        Items.clear();
        Selection.Clear();
    }

    // Logic would be written in the main code BeginChild() and outputting to local variables.
    // We extracted it into a function so we can call it easily from multiple places.
    void UpdateLayoutSizes(float avail_width)
    {
        // Layout: when not stretching: allow extending into right-most spacing.
        LayoutItemSpacing = (float)IconSpacing;
        if (StretchSpacing == false)
            avail_width += floorf(LayoutItemSpacing * 0.5f);

        // Layout: calculate number of icon per line and number of lines
        LayoutItemSize = ImVec2(floorf(IconSize), floorf(IconSize));
        LayoutColumnCount = std::max((int)(avail_width / (LayoutItemSize.x + LayoutItemSpacing)), 1);
        LayoutLineCount = (Items.Size + LayoutColumnCount - 1) / LayoutColumnCount;

        // Layout: when stretching: allocate remaining space to more spacing. Round before division, so item_spacing may be non-integer.
        if (StretchSpacing && LayoutColumnCount > 1)
            LayoutItemSpacing = floorf(avail_width - LayoutItemSize.x * LayoutColumnCount) / LayoutColumnCount;

        LayoutItemStep = ImVec2(LayoutItemSize.x + LayoutItemSpacing, LayoutItemSize.y + LayoutItemSpacing);
        LayoutSelectableSpacing = std::max(floorf(LayoutItemSpacing) - IconHitSpacing, 0.0f);
        LayoutOuterPadding = floorf(LayoutItemSpacing * 0.5f);
    }

    void Draw(const char* title, bool* p_open);
};


std::filesystem::path assetBaseDirectory;
std::filesystem::path assetImportedDirectory;
std::filesystem::path currentDirectory;
ExampleAssetsBrowser assets_browser;

void
ExampleAssetsBrowser::Draw(const char* title, bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(IconSize * 25, IconSize * 15), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(title, p_open, ImGuiWindowFlags_MenuBar))
    {
        ImGui::End();
        return;
    }

    // Menu bar
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Add 10000 items"))
                AddItems(10000);
            if (ImGui::MenuItem("Clear items"))
                ClearItems();
            ImGui::Separator();
            if (ImGui::MenuItem("Close", NULL, false, p_open != NULL))
                *p_open = false;
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
        ImGui::EndMenuBar();
    }

    if (currentDirectory != assetBaseDirectory)
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
            ImGui::PushID(path.string().c_str());

            bool selected = Selection.Contains((ImGuiID)name);
            bool clicked = ImGui::Selectable(name, selected);

            if (ImGui::IsItemHovered())
            {
                if (clicked)
                    assets::ViewImportSettings(content::GetAssetTypeFromExtension(path.extension().string().data()));
                if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    content::ImportAsset(path);
            }

            if (path.extension() == ".model")
            {
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                {
                    editor::assets::DropModelIntoScene(path);
                }
            }

            if (path.extension() == ".etex")
            {
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    editor::texture::OpenTextureView(path);
                }
            }

            ImGui::PopID();
        }
    }

    ImGui::SetNextWindowContentSize(ImVec2(0.0f, LayoutOuterPadding + LayoutLineCount * (LayoutItemSize.y + LayoutItemSpacing)));
    if (ImGui::BeginChild("Assets", ImVec2(0.0f, -ImGui::GetTextLineHeightWithSpacing()), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoMove))
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        const float avail_width = ImGui::GetContentRegionAvail().x;
        UpdateLayoutSizes(avail_width);

        // Calculate and store start position.
        ImVec2 start_pos = ImGui::GetCursorScreenPos();
        start_pos = ImVec2(start_pos.x + LayoutOuterPadding, start_pos.y + LayoutOuterPadding);
        ImGui::SetCursorScreenPos(start_pos);

        // Multi-select
        ImGuiMultiSelectFlags ms_flags = ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid;

        // - Enable box-select (in 2D mode, so that changing box-select rectangle X1/X2 boundaries will affect clipped items)
        if (AllowBoxSelect)
            ms_flags |= ImGuiMultiSelectFlags_BoxSelect2d;

        // - This feature allows dragging an unselected item without selecting it (rarely used)
        if (AllowDragUnselected)
            ms_flags |= ImGuiMultiSelectFlags_SelectOnClickRelease;

        // - Enable keyboard wrapping on X axis
        // (FIXME-MULTISELECT: We haven't designed/exposed a general nav wrapping api yet, so this flag is provided as a courtesy to avoid doing:
        //    ImGui::NavMoveRequestTryWrapping(ImGui::GetCurrentWindow(), ImGuiNavMoveFlags_WrapX);
        // When we finish implementing a more general API for this, we will obsolete this flag in favor of the new system)
        ms_flags |= ImGuiMultiSelectFlags_NavWrapX;

        ImGuiMultiSelectIO* ms_io = ImGui::BeginMultiSelect(ms_flags, Selection.Size, Items.Size);

        // Use custom selection adapter: store ID in selection (recommended)
        Selection.UserData = this;
        Selection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self_, int idx) { ExampleAssetsBrowser* self = (ExampleAssetsBrowser*)self_->UserData; return self->Items[idx].ID; };
        Selection.ApplyRequests(ms_io);

        const bool want_delete = (ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_Repeat) && (Selection.Size > 0)) || RequestDelete;
        const int item_curr_idx_to_focus = want_delete ? Selection.ApplyDeletionPreLoop(ms_io, Items.Size) : -1;
        RequestDelete = false;

        // Push LayoutSelectableSpacing (which is LayoutItemSpacing minus hit-spacing, if we decide to have hit gaps between items)
        // Altering style ItemSpacing may seem unnecessary as we position every items using SetCursorScreenPos()...
        // But it is necessary for two reasons:
        // - Selectables uses it by default to visually fill the space between two items.
        // - The vertical spacing would be measured by Clipper to calculate line height if we didn't provide it explicitly (here we do).
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(LayoutSelectableSpacing, LayoutSelectableSpacing));

        // Rendering parameters
        const ImU32 icon_type_overlay_colors[3] = { 0, IM_COL32(200, 70, 70, 255), IM_COL32(70, 170, 70, 255) };
        const ImU32 icon_bg_color = ImGui::GetColorU32(IM_COL32(35, 35, 35, 220));
        const ImVec2 icon_type_overlay_size = ImVec2(4.0f, 4.0f);
        const bool display_label = (LayoutItemSize.x >= ImGui::CalcTextSize("999").x);

        const int column_count = LayoutColumnCount;
        ImGuiListClipper clipper;
        clipper.Begin(LayoutLineCount, LayoutItemStep.y);
        if (item_curr_idx_to_focus != -1)
            clipper.IncludeItemByIndex(item_curr_idx_to_focus / column_count); // Ensure focused item line is not clipped.
        if (ms_io->RangeSrcItem != -1)
            clipper.IncludeItemByIndex((int)ms_io->RangeSrcItem / column_count); // Ensure RangeSrc item line is not clipped.
        while (clipper.Step())
        {
            for (int line_idx = clipper.DisplayStart; line_idx < clipper.DisplayEnd; line_idx++)
            {
                const int item_min_idx_for_current_line = line_idx * column_count;
                const int item_max_idx_for_current_line = std::min((line_idx + 1) * column_count, Items.Size);
                for (int item_idx = item_min_idx_for_current_line; item_idx < item_max_idx_for_current_line; ++item_idx)
                {
                    ExampleAsset* item_data = &Items[item_idx];
                    ImGui::PushID((int)item_data->ID);

                    // Position item
                    ImVec2 pos = ImVec2(start_pos.x + (item_idx % column_count) * LayoutItemStep.x, start_pos.y + line_idx * LayoutItemStep.y);
                    ImGui::SetCursorScreenPos(pos);

                    ImGui::SetNextItemSelectionUserData(item_idx);
                    bool item_is_selected = Selection.Contains((ImGuiID)item_data->ID);
                    bool item_is_visible = ImGui::IsRectVisible(LayoutItemSize);
                    ImGui::Selectable("", item_is_selected, ImGuiSelectableFlags_None, LayoutItemSize);

                    // Update our selection state immediately (without waiting for EndMultiSelect() requests)
                    // because we use this to alter the color of our text/icon.
                    if (ImGui::IsItemToggledSelection())
                        item_is_selected = !item_is_selected;

                    // Focus (for after deletion)
                    if (item_curr_idx_to_focus == item_idx)
                        ImGui::SetKeyboardFocusHere(-1);

                    // Drag and drop
                    if (ImGui::BeginDragDropSource())
                    {
                        // Create payload with full selection OR single unselected item.
                        // (the later is only possible when using ImGuiMultiSelectFlags_SelectOnClickRelease)
                        if (ImGui::GetDragDropPayload() == NULL)
                        {
                            ImVector<ImGuiID> payload_items;
                            void* it = NULL;
                            ImGuiID id = 0;
                            if (!item_is_selected)
                                payload_items.push_back(item_data->ID);
                            else
                                while (Selection.GetNextSelectedItem(&it, &id))
                                    payload_items.push_back(id);
                            ImGui::SetDragDropPayload("ASSETS_BROWSER_ITEMS", payload_items.Data, (size_t)payload_items.size_in_bytes());
                        }

                        // Display payload content in tooltip, by extracting it from the payload data
                        // (we could read from selection, but it is more correct and reusable to read from payload)
                        const ImGuiPayload* payload = ImGui::GetDragDropPayload();
                        const int payload_count = (int)payload->DataSize / (int)sizeof(ImGuiID);
                        ImGui::Text("%d assets", payload_count);

                        ImGui::EndDragDropSource();
                    }

                    // Render icon (a real app would likely display an image/thumbnail here)
                    // Because we use ImGuiMultiSelectFlags_BoxSelect2d, clipping vertical may occasionally be larger, so we coarse-clip our rendering as well.
                    if (item_is_visible)
                    {
                        ImVec2 box_min(pos.x - 1, pos.y - 1);
                        ImVec2 box_max(box_min.x + LayoutItemSize.x + 2, box_min.y + LayoutItemSize.y + 2); // Dubious
                        draw_list->AddRectFilled(box_min, box_max, icon_bg_color); // Background color
                        if (ShowTypeOverlay && item_data->Type != 0)
                        {
                            ImU32 type_col = icon_type_overlay_colors[item_data->Type % IM_ARRAYSIZE(icon_type_overlay_colors)];
                            draw_list->AddRectFilled(ImVec2(box_max.x - 2 - icon_type_overlay_size.x, box_min.y + 2), ImVec2(box_max.x - 2, box_min.y + 2 + icon_type_overlay_size.y), type_col);
                        }
                        if (display_label)
                        {
                            ImU32 label_col = ImGui::GetColorU32(item_is_selected ? ImGuiCol_Text : ImGuiCol_TextDisabled);
                            char label[32];
                            sprintf_s(label, "%d", item_data->ID);
                            draw_list->AddText(ImVec2(box_min.x, box_max.y - ImGui::GetFontSize()), label_col, label);
                        }
                    }

                    ImGui::PopID();
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

        ms_io = ImGui::EndMultiSelect();
        Selection.ApplyRequests(ms_io);
        if (want_delete)
            Selection.ApplyDeletionPostLoop(ms_io, Items, item_curr_idx_to_focus);
    }
    ImGui::EndChild();

    ImGui::Text("Selected: %d/%d items", Selection.Size, Items.Size);
    ImGui::End();
}

}

bool
InitializeAssetBrowserGUI()
{
    auto assetPath = editor::project::GetAssetPath();
    if (!std::filesystem::exists(assetPath))
    {
        log::Error("Asset directory does not exist: %s", assetPath.string());
        return false;
    }
    assetBaseDirectory = assetPath;
    assetImportedDirectory = assetPath / "Imported";
    currentDirectory = assetPath;

    for (auto& dir : std::filesystem::directory_iterator{ assetPath })
    {
		auto name = dir.path().filename().string();
		log::Info("found asset: %s", name.c_str());
    }

    //content::GeneratePrimitiveMeshAsset(content::PrimitiveMeshInfo{ content::PrimitiveMeshType::Plane });
	//std::unique_ptr<u8[]> data;
	//u64 size;
	//content::ReadAssetFile(assetBaseDirectory / "Generated" / "plane.geom", data, size, content::AssetType::Mesh);
	//log::Info("Loaded asset size: %u", size);

    //content::GeneratePrimitiveMeshAsset(content::PrimitiveMeshInfo{ content::PrimitiveMeshType::Cube });
    //std::unique_ptr<u8[]> data;
    //u64 size;
    //content::ReadAssetFile(assetBaseDirectory / "Generated" / "cube.geom", data, size, content::AssetType::Mesh);
    //log::Info("Loaded asset size: %u", size);

    return true;
}

void 
RenderAssetBrowserGUI()
{
	assets_browser.Draw("Asset Browser", nullptr);
}

}