#pragma once
#include "CommonHeaders.h"
#include "imgui.h"

namespace mofu::graphics {
struct PlatformInterface;
}

namespace mofu::graphics::ui {
//struct MaterialInfo
//{
//	id_t MaterialID{ id::INVALID_ID };
//	id_t* TextureIDs{ nullptr };
//
//};

bool Initialize(const PlatformInterface* const platform);
void Shutdown();

void StartNewFrame();
void ViewTexture(id_t textureID);
void DestroyViewTexture(id_t textureID);
[[nodiscard]] ImTextureID GetImTextureID(id_t textureID, u32 mipLevel = 0, u32 format = 0);
[[nodiscard]] ImTextureID GetImTextureID(id_t textureID, u32 arrayIndex, u32 mipLevel, u32 depthIndex, u32 format, bool isCubemap = false);
[[nodiscard]] id_t AddIcon(const u8* const blob);
}