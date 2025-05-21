#include "D3D12Material.h"

namespace mofu::graphics::d3d12::content::material {
namespace {

Vec<ID3D12RootSignature*> rootSignatures{};
std::unordered_map<u64, id_t> matRootSigMap{}; // maps a material's type and shader flags to an index in the root signature array (materials can share the same keys)
util::FreeList<std::unique_ptr<u8[]>> materials{};
std::mutex materialMutex{};

} // anonymous namespace

void 
GetMaterials(const id_t* const materialIds, u32 materialCount, const MaterialsCache& cache, u32& outDescriptorIndexCount)
{
}

id_t 
AddMaterial(const MaterialInitInfo& info)
{
    return id_t();
}

void 
RemoveMaterial(id_t id)
{
}

}