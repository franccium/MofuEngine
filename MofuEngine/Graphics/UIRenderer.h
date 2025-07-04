#pragma once
#include "CommonHeaders.h"

namespace mofu::graphics {
struct PlatformInterface;
}

namespace mofu::graphics::ui {

bool Initialize(const PlatformInterface* const platform);
void Shutdown();

void StartNewFrame();
void ViewTexture(id_t textureID);
void DestroyViewTexture(id_t textureID);
u64 GetImTextureID(id_t textureID);
}