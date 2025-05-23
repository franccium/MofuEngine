#include "D3D12Texture.h"

namespace mofu::graphics::d3d12::content::texture {
namespace {

util::FreeList<D3D12Texture> textures{};
util::FreeList<u32> descriptorIndices{};
std::mutex textureMutex{};

} // anonymous namespace

id_t 
AddTexture(const u8* const blob)
{
    return id_t();
}

void 
RemoveTexture(id_t id)
{
}

void 
GetDescriptorIndices(const id_t* const textureIDs, u32 idCount, u32* const outIndices)
{
}

}