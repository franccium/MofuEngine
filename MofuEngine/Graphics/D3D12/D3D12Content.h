#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::content {

namespace render_item {
struct RenderItemsCache
{
	id_t* const EntityIDs;
	id_t* const SubmeshGpuIDs;
	id_t* const MaterialIDS;
	ID3D12PipelineState** const GpassPso;
	ID3D12PipelineState** const DepthPso;	
};

id_t AddRenderItem(id_t entityID, id_t geometryContentID, u32 materialCount, const id_t* const materialIDs);
void RemoveRenderItem(id_t id);

void GetRenderItemIds(const FrameInfo& frameInfo, Vec<id_t>& outIds);
void GetRenderItems(const id_t* const itemIds, u32 idCount, const RenderItemsCache& cache);
} // namespace render_item

}