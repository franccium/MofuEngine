#pragma once
#include "../D3D12CommonHeaders.h"
#include "Graphics/Renderer.h"

namespace mofu::graphics::d3d12::content::material {
struct MaterialsCache
{
	ID3D12RootSignature** const RootSignatures;
	MaterialType::type* const MaterialTypes;
	u32** const DescriptorIndices;
	u32* const TextureCounts;
	MaterialSurface** const MaterialSurfaces;
};

void GetMaterials(const id_t* const materialIds, u32 materialCount, const MaterialsCache& cache, u32& outDescriptorIndexCount);
id_t AddMaterial(const MaterialInitInfo& info);
void RemoveMaterial(id_t id);
}