#include "D3D12Geometry.h"
#include "Utilities/IOStream.h"
#include "Graphics/Renderer.h"
#include "D3D12ContentCommon.h"

#include "Graphics/GeometryData.h"

namespace mofu::graphics::d3d12::content::geometry {
namespace {
struct SubmeshView
{
	D3D12_VERTEX_BUFFER_VIEW positionBufferView{};
	D3D12_VERTEX_BUFFER_VIEW elementBufferView{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	D3D12_PRIMITIVE_TOPOLOGY primitiveTopology{};
	u32 elementType{};
};

struct RTVertex
{
	v3 Position;
	u8 TNSign; // bit 0: tangent handedness, bit 1: tangent.z sign, bit 2: normal.z sign (0 for -1, 1 for +1)
	v2 UV;
	u16 Normal[2]; // normal packed as xy, reconstruct with normal.z sign
	u16 Tangent[2];
};

util::FreeList<DXResource*> submeshBuffers{};
util::FreeList<SubmeshView> submeshViews{};
std::mutex submeshMutex{};

//TODO: optimize
Vec<RTVertex> _globalVertexData;
Vec<u8> _globalIndexDataBlob{};
StructuredBuffer _globalVertexBuffer{};
FormattedBuffer _globalIndexBuffer{};
util::FreeList<MeshInfo> _meshInfos{};

} // anonymous namespace

bool
Initialize()
{
#if RAYTRACING
	_globalVertexData = Vec<RTVertex>{};
	//_globalVertexData.reserve(10000);
	_globalIndexDataBlob = Vec<u8>{};
	//_globalIndexDataBlob.reserve(256 * 1024 * 1024);
#endif
	return true;
}

void
CreateGlobalBuffers()
{
	StructuredBufferInitInfo info{};
	info.Stride = sizeof(RTVertex);
	info.ElementCount = _globalVertexData.size();
	info.InitialData = _globalVertexData.data();
	info.Name = L"Global RT Vertex Buffer";
	_globalVertexBuffer.Initialize(info);
	FormattedBufferInitInfo indexInfo{};
	indexInfo.Format = DXGI_FORMAT_R16_UINT; // TODO: R32_UINT
	indexInfo.ElementCount = _globalIndexDataBlob.size() / sizeof(u16); //TODO: indexSize
	indexInfo.InitialData = _globalIndexDataBlob.data();
	indexInfo.Name = L"Global RT Index Buffer";
	_globalIndexBuffer.Initialize(indexInfo);
}

MeshInfo GetMeshInfo(id_t meshID)
{
	assert(meshID < _meshInfos.size());
	return _meshInfos[meshID];
}

const StructuredBuffer& GlobalVertexBuffer()
{
	return _globalVertexBuffer;
}

const FormattedBuffer& GlobalIndexBuffer()
{
	return _globalIndexBuffer;
}

const util::FreeList<MeshInfo>& GlobalMeshInfos()
{
	return _meshInfos;
}

u32 GlobalMeshCount()
{
	return submeshViews.size();
}

void
GetSubmeshViews(const id_t* const gpuIds, u32 idCount, const SubmeshViewsCache& cache)
{
	//assert(gpuIds && idCount > 0);
	//assert(cache.PositionBuffers&& cache.ElementBuffers&& cache.IndexBufferViews);

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

#if RAYTRACING
	//FIXME: RTTEST
	_globalVertexBuffer.Release();
	_globalIndexBuffer.Release();
	assert(elementSize == sizeof(mofu::content::StaticNormalTexture));
	MeshInfo meshInfo{};
	meshInfo.VertexCount = vertexCount;
	//meshInfo.VertexGlobalOffset = (u32)(_globalVertexData.size() / sizeof(RTVertex));
	meshInfo.IndexCount = indexCount;
	//meshInfo.IndexGlobalOffset = (u32)(_globalIndexDataBlob.size() / indexSize);

	const u32 lastVt{ (u32)_globalVertexData.size() };
	_globalVertexData.resize(_globalVertexData.size() + vertexCount);
	meshInfo.VertexGlobalOffset = lastVt;
	for (u32 i{ 0 }; i < vertexCount; ++i)
	{
		v3 pos{ ((v3*)reader.Position())[i] };
		const mofu::content::StaticNormalTexture& snt{ ((mofu::content::StaticNormalTexture*)(reader.Position() + alignedPositionBufferSize))[i] };
		new (&_globalVertexData[lastVt + i]) RTVertex{ pos, snt.TNSign, snt.UV, snt.Normal[0], snt.Normal[1], snt.tangent[0], snt.tangent[1]};
	}

	const u32 lastIdx{ (u32)_globalIndexDataBlob.size() };
	_globalIndexDataBlob.resize(_globalIndexDataBlob.size() + indexBufferSize);
	meshInfo.IndexGlobalOffset = lastIdx;
	memcpy(_globalIndexDataBlob.data() + lastIdx, reader.Position() + alignedPositionBufferSize + alignedElementBufferSize, indexBufferSize);

	_meshInfos.add(meshInfo);

	//if(_globalVertexData.capacity() - _globalVertexData.size() < alignedElementBufferSize / elementSize)
	//	_globalVertexData.reserve(_globalVertexData.size() + alignedElementBufferSize / elementSize + 1000);
	//if (_globalIndexDataBlob.capacity() - _globalIndexDataBlob.size() < indexBufferSize)
	//	_globalIndexDataBlob.reserve(_globalIndexDataBlob.size() + indexBufferSize + 16 * 1024 * 1024);
	//RTVertex* const vertexDataPtr{ _globalVertexData.data() + _globalVertexData.size() };
	//u8* const indexDataPtr{ _globalIndexDataBlob.data() + _globalIndexDataBlob.size() };
	//memcpy(vertexDataPtr, reader.Position() + alignedPositionBufferSize, alignedElementBufferSize); // FIXME: aligned?
	//for (u32 i{ 0 }; i < vertexCount; ++i)
	//{
	//	v3& pos{ ((v3*)reader.Position())[i] };
	//	vertexDataPtr[i].Position = pos;
	//}
	//const u64 newVertexCount{ _globalVertexData.size() + vertexCount };
	//const u64 newIndexCount{ _globalIndexDataBlob.size() + indexCount };
	//_globalVertexData.SetEnd(newVertexCount);
	//memcpy(indexDataPtr, reader.Position() + alignedPositionBufferSize + alignedElementBufferSize, indexBufferSize);
	//_globalIndexDataBlob.SetEnd(newIndexCount);
	
	// RTTEST
#endif

	DXResource* resource{ d3dx::CreateResourceBuffer(reader.Position(), totalBufferSize) };

	//auto rawPtr = reader.Position() + alignedPositionBufferSize;
	//const mofu::content::StaticNormalTexture* debugPtr 
	//	= reinterpret_cast<const mofu::content::StaticNormalTexture*>(rawPtr);

	//mofu::content::StaticNormalTexture s0 = debugPtr[0];
	//mofu::content::StaticNormalTexture s1 = debugPtr[1];
	//mofu::content::StaticNormalTexture s2 = debugPtr[2];

	reader.Skip(totalBufferSize);
	// advance the data pointer past the submesh data
	blob = reader.Position();

	const D3D12_GPU_VIRTUAL_ADDRESS resourceAddress{ resource->GetGPUVirtualAddress() };

	SubmeshView submeshView{};
	submeshView.positionBufferView.BufferLocation = resourceAddress;
	submeshView.positionBufferView.SizeInBytes = positionBufferSize;
	submeshView.positionBufferView.StrideInBytes = sizeof(v3);

	if (elementSize != 0)
	{
		submeshView.elementBufferView.BufferLocation = resourceAddress + alignedPositionBufferSize;
		submeshView.elementBufferView.SizeInBytes = elementBufferSize;
		submeshView.elementBufferView.StrideInBytes = elementSize;
	}

	submeshView.indexBufferView.BufferLocation = resourceAddress + alignedPositionBufferSize + alignedElementBufferSize;
	submeshView.indexBufferView.SizeInBytes = indexBufferSize;
	submeshView.indexBufferView.Format = (indexSize == sizeof(u16)) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

	submeshView.elementType = elementType;
	submeshView.primitiveTopology = D3D12_PRIMITIVE_TOPOLOGIES[primitiveTopology];

	std::lock_guard lock{ submeshMutex };
	submeshBuffers.add(resource);
	return submeshViews.add(submeshView);
}

void 
RemoveSubmesh(id_t id)
{
	std::lock_guard lock{ submeshMutex };
	submeshViews.remove(id);
	core::DeferredRelease(submeshBuffers[id]);
	submeshBuffers.remove(id);
#if RAYTRACING
	_meshInfos.remove(id);
#endif
}

}