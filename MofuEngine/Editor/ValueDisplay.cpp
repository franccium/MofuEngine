#include "ValueDisplay.h"

namespace mofu::editor {
namespace {
constexpr f32 DRAG_SPEED_FACTOR{ 1000.f };


}

void DisplayLabelT(const char* label)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::TableNextColumn();
}

bool DisplayEditableBool(bool* v, const char* label)
{
	DisplayLabelT(label);
	ImGui::PushID(v);
	bool changed{ ImGui::Checkbox(label, v) };
	ImGui::PopID();
	return changed;
}

bool DisplayEditableUintNT(u32* v, const char* label, u32 minVal, u32 maxVal)
{
	ImGui::PushID(v);
	ImGui::SetNextItemWidth(-FLT_MIN);
	bool changed{ ImGui::DragScalar("##Editor", ImGuiDataType_U32, v, 1.0f, &minVal, &maxVal) };
	ImGui::PopID();
	return changed;
}

bool DisplayEditableUint(u32* v, const char* label, u32 minVal, u32 maxVal)
{
	DisplayLabelT(label);
	ImGui::PushID(v);
	ImGui::SetNextItemWidth(-FLT_MIN);
	bool changed{ ImGui::DragScalar("##Editor", ImGuiDataType_U32, v, 1.0f, &minVal, &maxVal) };
	ImGui::PopID();
	return changed;
}

bool DisplayEditableFloat(f32* v, const char* label, f32 minVal, f32 maxVal, const char* format)
{
	DisplayLabelT(label);
	ImGui::PushID(v);
	f32 dragSpeed = (maxVal - minVal) / DRAG_SPEED_FACTOR;
	bool changed{ ImGui::DragFloat("##Editor", (f32*)v, dragSpeed, minVal, maxVal, format) };
	ImGui::PopID();
	return changed;
}

bool DisplayEditableVector2(v2* v, const char* label, f32 minVal, f32 maxVal)
{
	DisplayLabelT(label); 
	ImGui::PushID(v);
	ImGui::SetNextItemWidth(-FLT_MIN);
	f32 dragSpeed = (maxVal - minVal) / DRAG_SPEED_FACTOR;
	bool changed{ ImGui::DragFloat2("##Editor", (f32*)v, dragSpeed, minVal, maxVal, "%.3f") };
	ImGui::PopID();
	return changed;
}

bool DisplayEditableVector3(v3* v, const char* label, f32 minVal, f32 maxVal, const char* format)
{
	DisplayLabelT(label);
	ImGui::PushID(v);
	ImGui::SetNextItemWidth(-FLT_MIN);
	f32 dragSpeed = (maxVal - minVal) / DRAG_SPEED_FACTOR;
	bool changed{ ImGui::DragFloat3("##Editor", (f32*)v, dragSpeed, minVal, maxVal, format) };
	ImGui::PopID();
	return changed;
}

bool DisplayEditableVector4(v4* v, const char* label, f32 minVal, f32 maxVal)
{
	DisplayLabelT(label); 
	ImGui::PushID(v);
	f32 dragSpeed = (maxVal - minVal) / DRAG_SPEED_FACTOR;
	bool changed{ ImGui::DragFloat4("##Editor", (f32*)v, dragSpeed, minVal, maxVal, "%.3f") };
	ImGui::PopID();
	return changed;
}

bool DisplayEditableMatrix4x4(m4x4* m, const char* label)
{
	ImGui::TextUnformatted(label);
	ImGui::PushID(m);
	bool changed{ ImGui::InputFloat4("##r1", &m->_11) };
	changed |= ImGui::InputFloat4("##r2", &m->_21);
	changed |= ImGui::InputFloat4("##r3", &m->_31);
	changed |= ImGui::InputFloat4("##r4", &m->_41);
	ImGui::PopID();
	return changed;
}

void DisplayMatrix4x4(const m4x4& m, const char* label)
{
	ImGui::TextUnformatted(label);
	ImGui::Text("%.3f", m._11);
	ImGui::Text("%.3f", m._21);
	ImGui::Text("%.3f", m._31);
	ImGui::Text("%.3f", m._41);
}

void DisplayUint(u32 v, const char* label)
{
	DisplayLabelT(label);
	ImGui::Text("%u", v);
}

void DisplayFloatT(f32 v, const char* label)
{
	DisplayLabelT(label);
	ImGui::Text("%.3f", v);
}

void DisplayFloat(f32 v, const char* label)
{
	ImGui::TextUnformatted(label); 
	ImGui::SameLine();
	ImGui::Text("%.3f", v);
}

void DisplayVector2(v2 v, const char* label)
{
	ImGui::BeginTable(label, 3);
	DisplayLabelT(label);
	ImGui::Text("%.3f", v.x);
	ImGui::NextColumn();
	ImGui::Text("%.3f", v.y);
	ImGui::EndTable();
}

void DisplayVector3(const v3& v, const char* label)
{
	ImGui::BeginTable(label, 4);
	DisplayLabelT(label);
	ImGui::Text("%.3f", v.x);
	ImGui::NextColumn();
	ImGui::Text("%.3f", v.y);
	ImGui::NextColumn();
	ImGui::Text("%.3f", v.z);
	ImGui::EndTable();
}

void DisplayVector4(const v4& v, const char* label)
{
	ImGui::BeginTable(label, 5);
	DisplayLabelT(label);
	ImGui::Text("%.3f", v.x);
	ImGui::NextColumn();
	ImGui::Text("%.3f", v.y);
	ImGui::NextColumn();
	ImGui::Text("%.3f", v.z);
	ImGui::NextColumn();
	ImGui::Text("%.3f", v.w);
	ImGui::EndTable();
}

void DisplaySliderUint(const char* label, u32* v, u32 minVal, u32 maxVal)
{
	ImGui::SliderScalar(label, ImGuiDataType_U32, v, &minVal, &maxVal);
}

}