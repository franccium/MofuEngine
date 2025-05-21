#include "D3D12Content.h"
#include "Content/D3D12Geometry.h"
#include "Content/D3D12Material.h"
#include "Content/D3D12Texture.h"
#include "Content/ResourceCreation.h"

namespace mofu::graphics::d3d12::content {
namespace {

struct PsoID
{
	id_t GPassPsoID;
	id_t DepthPsoID;
};

struct D3D12RenderItem
{
	id_t EntityID; // access the rendered object's transform
	id_t SubmeshGpuID; // to get vertex and index buffers
	id_t MaterialID;
	id_t GPassPsoID;
	id_t DepthPsoID;
};

// since GetRenderItemIds is called every frame at least once, we want to cache the results
struct
{
	Vec<mofu::content::LodOffset> LODOffsets;
	Vec<id_t> GeometryIDs;
	Vec<f32> LODThresholds;
} FrameCache;

util::FreeList<D3D12RenderItem> renderItems{};
util::FreeList<std::unique_ptr<id_t[]>> renderItemIds{};
std::mutex renderItemMutex{};

Vec<ID3D12PipelineState*> pipelineStates;
std::unordered_map<u64, id_t> psoMap;
std::mutex psoMutex{};

} // anonymous namespace

namespace render_item {
id_t 
AddRenderItem(id_t entityID, id_t geometryContentID, u32 materialCount, const id_t* const materialIDs)
{
	return id_t{};
}

void 
RemoveRenderItem(id_t id)
{
}

void 
GetRenderItemIds(const FrameInfo& frameInfo, Vec<id_t>& outIds)
{
}

void 
GetRenderItems(const id_t* const itemIds, u32 idCount, const RenderItemsCache& cache)
{
}
} // namespace render_item

}