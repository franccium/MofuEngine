#include "FontView.h"
#include "Physics/DebugRenderer/FontRenderer.h"

namespace mofu::editor::font {
namespace {
bool _isOpen{ false };
id_t _fontTexID{ U32_INVALID_ID };
ImTextureID _fontTexImID{};
v2 _fontTexSize{};
std::string _fontName{};
} // anmonymous namespace

void 
Initialize()
{
	_fontTexID = graphics::d3d12::debug::font::GetFontTextureID();
	assert(id::IsValid(_fontTexID));
	_fontTexSize = graphics::d3d12::debug::font::GetFontTextureSize();
	_fontName = graphics::d3d12::debug::font::GetFontName();
	_fontTexImID = graphics::ui::GetImTextureID(graphics::d3d12::debug::font::GetFontTextureID());
}

void 
DisplayFontView()
{
	if (!_isOpen) return;

	ImGui::Begin("Font View", &_isOpen);

	if (id::IsValid(_fontTexID))
	{
		ImGui::Text("Font: %s", _fontName.c_str());
		ImGui::Text("Texture Size: (%f, %f)", _fontTexSize.x, _fontTexSize.y);
		v2 size{ graphics::d3d12::debug::font::GetFontTextureSize() };
		ImGui::Image(_fontTexImID, ImVec2(size.x, size.y));
	}

	ImGui::End();
}

void
OpenFontView()
{
	_isOpen = true;
}

}