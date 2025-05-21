#pragma once
#include "../D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::content::geometry {

// helps turn the scaterred buffers and AoS into SoA for faster access
struct SubmeshViewsCache
{
	D3D12_GPU_VIRTUAL_ADDRESS* const PositionBuffers;
	D3D12_GPU_VIRTUAL_ADDRESS* const ElementBuffers;
	D3D12_INDEX_BUFFER_VIEW* const IndexBufferViews;
	D3D12_PRIMITIVE_TOPOLOGY* const PrimitiveTopologies;
	u32* const ElementTypes;
};

struct ElementViewsCache {};

void GetViews(const id_t* const gpuIds, u32 idCount, const SubmeshViewsCache& cache);
id_t AddSubmesh(const u8*& blob);
void RemoveSubmesh(id_t id);

}