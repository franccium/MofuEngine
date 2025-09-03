#pragma once
#include "CommonHeaders.h"
#include "imgui.h"

namespace mofu::editor::ui {
struct Color
{
	constexpr static ImVec4 RED{ 0.8f, 0.2f, 0.2f, 1.f };
	constexpr static ImVec4 GREEN{ 0.8f, 0.8f, 0.2f, 1.f };
	constexpr static ImVec4 BLUE{ 0.2f, 0.2f, 0.8f, 1.f };
	constexpr static ImVec4 YELLOW{ 0.8f, 0.8f, 0.2f, 1.f };
	constexpr static ImVec4 WHITE{ 0.9f, 0.9f, 0.9f, 1.f };
	constexpr static ImVec4 VWHITE{ 1.f, 1.f, 1.f, 1.f };
	constexpr static ImVec4 VRED{ 1.f, 0.f, 0.f, 1.f };
};

// useful for ImGui::TextUnformatted or many text calls, otherwise ImGui::TextColored() is fine
_ALWAYS_INLINE void PushTextColor(ImVec4 color) { ImGui::PushStyleColor(ImGuiCol_Text, color); } 
_ALWAYS_INLINE void PopTextColor() { ImGui::PopStyleColor(); }
_ALWAYS_INLINE void TextColored(ImVec4 color, const char* text) 
{ 
	ImGui::PushStyleColor(ImGuiCol_Text, color); 
	ImGui::TextUnformatted(text); 
	ImGui::PopStyleColor(); 
}

}