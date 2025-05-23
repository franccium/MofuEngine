#pragma once
#include "../D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::content::texture {

id_t AddTexture(const u8* const blob);
void RemoveTexture(id_t id);
void GetDescriptorIndices(const id_t* const textureIDs, u32 idCount, u32* const outIndices);

}