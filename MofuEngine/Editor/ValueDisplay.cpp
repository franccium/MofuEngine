#include "ValueDisplay.h"

namespace mofu::editor {
namespace {
void DisplayLabel(const char* label)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::TableNextColumn();
}

}

void DisplayEditableUint(u32* v, const char* label, u32 minVal, u32 maxVal)
{
	DisplayLabel(label);
	ImGui::PushID(v);
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::DragScalarN("##Editor", ImGuiDataType_U32, v, 1, 1.0f, &minVal, &maxVal);
	ImGui::PopID();
}

void DisplayEditableFloat(f32* v, const char* label, f32 minVal, f32 maxVal)
{
	DisplayLabel(label);
	ImGui::PushID(v);
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::DragScalarN("##Editor", ImGuiDataType_Float, v, 1, 1.0f, &minVal, &maxVal);
	ImGui::PopID();
}

void DisplayEditableVector2(v2* v, const char* label, f32 minVal, f32 maxVal)
{
	DisplayLabel(label); 
	ImGui::PushID(v);
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::SliderScalarN("##Editor", ImGuiDataType_Float, v, 2, &minVal, &maxVal);
	ImGui::PopID();
}

void DisplayEditableVector3(v3* v, const char* label, f32 minVal, f32 maxVal)
{
	DisplayLabel(label);
	ImGui::PushID(v);
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::SliderScalarN("##Editor", ImGuiDataType_Float, v, 3, &minVal, &maxVal);
	ImGui::PopID();
}

void DisplayEditableVector4(v4* v, const char* label, f32 minVal, f32 maxVal)
{
	DisplayLabel(label); 
	ImGui::PushID(v);
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::SliderScalarN("##Editor", ImGuiDataType_Float, v, 4, &minVal, &maxVal);
	ImGui::PopID();
}

void DisplayMatrix4x4(m4x4* m, const char* label)
{
	ImGui::TextUnformatted(label);
	ImGui::PushID(m);
	ImGui::InputFloat4("##r1", &m->_11);
	ImGui::InputFloat4("##r2", &m->_21);
	ImGui::InputFloat4("##r3", &m->_31);
	ImGui::InputFloat4("##r4", &m->_41);
	ImGui::PopID();
}

void DisplayUint(u32 v, const char* label)
{
	DisplayLabel(label);
	ImGui::Text("%u", v);
}

void DisplayFloat(f32 v, const char* label)
{
	DisplayLabel(label);
	ImGui::Text("%.2f", v);
}

void DisplaySliderUint(const char* label, u32* v, u32 minVal, u32 maxVal)
{
	ImGui::SliderScalar(label, ImGuiDataType_U32, v, &minVal, &maxVal);
}

}