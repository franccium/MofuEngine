#include "D3D12Geometry.h"
#include "Utilities/IOStream.h"
#include "Graphics/Renderer.h"

namespace  mofu::graphics::d3d12::content::geometry {
namespace {
struct SubmeshView
{
	D3D12_VERTEX_BUFFER_VIEW positionBufferView{};
	D3D12_VERTEX_BUFFER_VIEW elementBufferView{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	D3D12_PRIMITIVE_TOPOLOGY primitiveTopology{};
	u32 elementType{};
};

constexpr D3D12_PRIMITIVE_TOPOLOGY D3D12_PRIMITIVE_TOPOLOGIES[PrimitiveTopology::Count]
{
	D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
	D3D_PRIMITIVE_TOPOLOGY_LINELIST,
	D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,
	D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
};

constexpr D3D12_PRIMITIVE_TOPOLOGY_TYPE D3D12_PRIMITIVE_TOPOLOGY_TYPES[PrimitiveTopology::Count]
{
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE
};

util::FreeList<DXResource*> submeshBuffers{};
util::FreeList<SubmeshView> submeshViews{};
std::mutex submeshMutex{};

} // anonymous namespace

void
GetViews(const id_t* const gpuIds, u32 idCount, const SubmeshViewsCache& cache)
{
	assert(gpuIds && idCount > 0);
	assert(cache.PositionBuffers&& cache.ElementBuffers&& cache.IndexBufferViews);

	std::lock_guard lock{ submeshMutex };
	for (u32 i{ 0 }; i < idCount; ++i)
	{
		const SubmeshView& view{ submeshViews[gpuIds[i]] };
		cache.PositionBuffers[i] = view.positionBufferView.BufferLocation;
		cache.ElementBuffers[i] = view.elementBufferView.BufferLocation;
		cache.IndexBufferViews[i] = view.indexBufferView;
		cache.PrimitiveTopologies[i] = view.primitiveTopology;
		cache.ElementTypes[i] = view.elementType;
	}
}

// Creates and aligned buffer from the submesh data and uploads it via an upload queue
// NOTE: expects data to contain:
// u32 element_size, u32 vertex_count
// u32 index_count, u32 elements_type, u32 primitive_topology
// u8 positions[sizeof(v3) * vertex_count], // sizeof(positions) should be a multiple of 4 bytes
// u8 elements[element_size * vertex_count], // sizeof(elements) should be a multiple of 4 bytes
// u8 indices[index_size * index_count]
// 
// Advances the data pointer
// position and element buffers have to be aligned to a multiple of 4 bytes (D3D12_STANDARD_MAXIMUM_ELEMENT_ALIGNMENT_BYTE_MULTIPLE)
id_t 
AddSubmesh(const u8*& blob)
{
	util::BlobStreamReader reader{ (const u8*)blob };

	const u32 elementSize{ reader.Read<u32>() };
	const u32 vertexCount{ reader.Read<u32>() };
	const u32 indexCount{ reader.Read<u32>() };
	const u32 elementType{ reader.Read<u32>() };
	const u32 primitiveTopology{ reader.Read<u32>() };	
	
	const u32 indexSize{ (vertexCount) < (1 << 16) ? sizeof(u16) : sizeof(u32) };
	const u32 positionBufferSize{ sizeof(v3) * vertexCount };
	const u32 elementBufferSize{ elementSize * vertexCount };
	const u32 indexBufferSize{ indexSize * indexCount };

	constexpr u32 alignment{ D3D12_STANDARD_MAXIMUM_ELEMENT_ALIGNMENT_BYTE_MULTIPLE };
	const u32 alignedPositionBufferSize{ (u32)math::AlignUp<alignment>(positionBufferSize) };
	const u32 alignedElementBufferSize{ (u32)math::AlignUp<alignment>(elementBufferSize) };
	const u32 totalBufferSize{ alignedPositionBufferSize + alignedElementBufferSize + indexBufferSize };

	DXResource* resource{ d3dx::CreateResourceBuffer(reader.Position(), totalBufferSize) };
	reader.Skip(totalBufferSize);
	// advance the data pointer past the submesh data
	blob = reader.Position();

	SubmeshView view{};
	view.positionBufferView.BufferLocation = resource->GetGPUVirtualAddress();
	view.positionBufferView.SizeInBytes = positionBufferSize;
	view.positionBufferView.StrideInBytes = sizeof(v3);

	if (elementSize != 0)
	{
		view.elementBufferView.BufferLocation = resource->GetGPUVirtualAddress() + alignedPositionBufferSize;
		view.elementBufferView.SizeInBytes = elementBufferSize;
		view.elementBufferView.StrideInBytes = elementSize;
	}

	view.indexBufferView.BufferLocation = resource->GetGPUVirtualAddress() + alignedPositionBufferSize + alignedElementBufferSize;
	view.indexBufferView.SizeInBytes = indexBufferSize;
	view.indexBufferView.Format = (indexSize == sizeof(u16)) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;	

	view.elementType = elementType;
	view.primitiveTopology = D3D12_PRIMITIVE_TOPOLOGIES[primitiveTopology];

	std::lock_guard lock{ submeshMutex };
	submeshBuffers.add(resource);
	return submeshViews.add(view);
}

void 
RemoveSubmesh(id_t id)
{
	std::lock_guard lock{ submeshMutex };
	submeshViews.remove(id);
	core::DeferredRelease(submeshBuffers[id]);
	submeshBuffers.remove(id);
}

}