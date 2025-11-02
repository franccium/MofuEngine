#pragma once
#include "Graphics/D3D12/D3D12CommonHeaders.h"
#include "Graphics/D3D12/D3D12Primitives.h"
#include "Physics/JoltCommon.h"
#include <Jolt/Math/Float2.h>

namespace mofu::graphics::d3d12::debug::font {
constexpr const char* FONTS_PATH{ "../MofuEngine/Content/EngineAssets/Fonts/" };
constexpr const char* FONT_NAMES[]{
    "Roboto-Regular.ttf",
    "m.ttf",
};
constexpr u32 FONT_COUNT{ _countof(FONT_NAMES) };
constexpr const char* DEFAULT_FONT{ FONT_NAMES[0] };

struct FontRenderer
{
    static constexpr u32 CHAR_COUNT{ 256 };
    static constexpr u8 FIRST_PRINTABLE_CHAR{ ' ' };
    static constexpr u8 LAST_PRINTABLE_CHAR{ 255 };
    static constexpr u8 PRINTABLE_CHAR_COUNT{ LAST_PRINTABLE_CHAR - FIRST_PRINTABLE_CHAR };

    bool Initialize(const char* fontName, u32 charHeight);
    void RenderText(const D3D12FrameInfo& frameInfo, D3D12_GPU_VIRTUAL_ADDRESS constants,
        JPH::Mat44Arg transform, std::string_view text, JPH::ColorArg color) const;

    struct FontVertex
    {
        JPH::Float3 Position{};
        JPH::Color Color{};
        JPH::Float2 UV{};
        JPH::Float2 _pad{};
    };

    struct FontRootParameterIndices
    {
        enum : u8
        {
            VSConstants = 0,
            FontTexture = 1,
            Count
        };
    };

    std::string FontName;
    u32 CharHeight{};
    u32 HorizontalTexelCount{};
    u32 VerticalTexelCount{};
    f32 UCorrection{};
    f32 VCorrection{};
    u16 StartU[PRINTABLE_CHAR_COUNT]{};
    u16 StartV[PRINTABLE_CHAR_COUNT]{};
    u8 CharWidths[PRINTABLE_CHAR_COUNT]{};
    u8 Spacing[PRINTABLE_CHAR_COUNT][PRINTABLE_CHAR_COUNT]{};

    id_t TextureID{};

    ID3D12RootSignature* RootSig{ nullptr };
    ID3D12PipelineState* PSO{ nullptr };
};

id_t GetFontTextureID();
v2 GetFontTextureSize();
std::string GetFontName();
}