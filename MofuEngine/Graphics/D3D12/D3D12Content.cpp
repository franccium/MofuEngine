#include "D3D12Content.h"
#include "D3D12Content/D3D12Geometry.h"
#include "D3D12Content/D3D12Material.h"
#include "D3D12Content/D3D12Texture.h"
#include "Content/ResourceCreation.h"
#include "D3D12Content/D3D12ContentCommon.h"
#include "D3D12GPass.h"

namespace mofu::graphics::d3d12::content {
namespace {

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
util::FreeList<std::unique_ptr<id_t[]>> renderItemIDs{};
std::mutex renderItemMutex{};

Vec<ID3D12RootSignature*> rootSignatures{};
std::unordered_map<u64, id_t> matRootSigMap{}; // maps a material's type and shader flags to an index in the root signature array (materials can share the same keys)
util::FreeList<std::unique_ptr<u8[]>> materials{};
std::mutex materialMutex{};

Vec<ID3D12PipelineState*> pipelineStates;
std::unordered_map<u64, id_t> psoMap;
std::mutex psoMutex{};

constexpr D3D12_ROOT_SIGNATURE_FLAGS
GetRootSignatureFlags(ShaderFlags::flags shaderFlags)
{
	D3D12_ROOT_SIGNATURE_FLAGS signature_flags{ d3dx::D3D12RootSignatureDesc::DEFAULT_FLAGS };
	if (shaderFlags & ShaderFlags::Vertex) signature_flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
	if (shaderFlags & ShaderFlags::Hull) signature_flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
	if (shaderFlags & ShaderFlags::Domain) signature_flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
	if (shaderFlags & ShaderFlags::Geometry) signature_flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
	if (shaderFlags & ShaderFlags::Pixel) signature_flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
	if (shaderFlags & ShaderFlags::Amplification) signature_flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS;
	if (shaderFlags & ShaderFlags::Mesh) signature_flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
	return signature_flags;
}

// return index of the first set bit, going from LSB to MSB
#pragma intrinsic(_BitScanForward)
ShaderType::type
GetShaderType(u32 flag)
{
	assert(flag);
	unsigned long index;
	_BitScanForward(&index, flag);
	return (ShaderType::type)index;
}

id_t
CreatePSOIfNeeded(const u8* const streamPtr, u64 alignedStreamSize, [[maybe_unused]] bool isDepth)
{
	const u64 key{ math::CRC32_u64(streamPtr, alignedStreamSize) };
	{
		std::lock_guard lock{ psoMutex };
		auto pair{ psoMap.find(key) };
		if (pair != psoMap.end())
		{
			return pair->second;
		}
	}

	d3dx::D3D12PipelineStateSubobjectStream* const stream{ (d3dx::D3D12PipelineStateSubobjectStream*)streamPtr };
	ID3D12PipelineState* pso{ d3dx::CreatePipelineState(stream, alignedStreamSize) };

	{
		std::lock_guard lock{ psoMutex };
		id_t id{ (id_t)pipelineStates.size() };
		pipelineStates.emplace_back(pso);
		psoMap[key] = id;
		NAME_D3D12_OBJECT_INDEXED(pso, key, isDepth ? L"Depth-only Pipeline State Object - key:" : L"GPass Pipeline State Object - key:");
		return id;
	}
}

struct PsoID
{
	id_t GPassPsoID;
	id_t DepthPsoID;
};

PsoID
CreatePSO(id_t materialID, D3D12_PRIMITIVE_TOPOLOGY primitiveTopology, u32 elementType)
{
	assert(id::IsValid(materialID));

	// if a hashcode with the same value already exists, we can use it as a key to get an existing PSO
	// we need to align the buffer to 8 bytes as we process CRT in chunks of 8
	constexpr u64 alignedStreamSize{ math::AlignUp<sizeof(u64)>(sizeof(d3dx::D3D12PipelineStateSubobjectStream)) };
	u8* const streamPtr{ (u8*)alloca(alignedStreamSize) };
	ZeroMemory(streamPtr, alignedStreamSize);

	new (streamPtr) d3dx::D3D12PipelineStateSubobjectStream{};
	d3dx::D3D12PipelineStateSubobjectStream& stream{ *(d3dx::D3D12PipelineStateSubobjectStream* const)streamPtr };

	{
		std::lock_guard lock{ materialMutex };
		const material::D3D12MaterialStream material{ materials[materialID].get() };

		D3D12_RT_FORMAT_ARRAY rtArray{};
		rtArray.NumRenderTargets = 1;
		rtArray.RTFormats[0] = gpass::MAIN_BUFFER_FORMAT;

		stream.rootSignature = rootSignatures[material.RootSignatureID()];
		stream.renderTargetFormats = rtArray;
		stream.primitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPES[primitiveTopology];
		stream.rasterizer = d3dx::RasterizerState.BACKFACE_CULLING;
		stream.depthStencil = d3dx::DepthState.REVERSED_READONLY;
		stream.depthStencilFormat = gpass::DEPTH_BUFFER_FORMAT;
		stream.blend = d3dx::BlendState.DISABLED;

		const ShaderFlags::flags shaderFlags{ material.ShaderFlags() };
		D3D12_SHADER_BYTECODE shaders[ShaderType::Count]{};
		u32 shaderIndex{ 0 };
		for (u32 i{ 0 }; i < ShaderType::Count; ++i)
		{
			if (shaderFlags & (1u << i))
			{
				// NOTE: each type of shader may have keys that are generated from submesh
				// or material properties, for now, only vertex shaders have different variations, depending on the elementType
				const u32 key{ GetShaderType(shaderFlags & (1u << i)) == ShaderType::Vertex ? elementType : U32_INVALID_ID };
				mofu::content::CompiledShaderPtr shader{ mofu::content::GetShader(material.ShaderIDs()[shaderIndex], key) };
				assert(shader);
				shaders[i].pShaderBytecode = shader->Bytecode();
				shaders[i].BytecodeLength = shader->BytecodeSize();
				++shaderIndex;
			}
		}

		stream.vs = shaders[ShaderType::Vertex];
		stream.ps = shaders[ShaderType::Pixel];
		stream.ds = shaders[ShaderType::Domain];
		stream.hs = shaders[ShaderType::Hull];
		stream.gs = shaders[ShaderType::Geometry];
		stream.cs = shaders[ShaderType::Compute];
		stream.as = shaders[ShaderType::Amplification];
		stream.ms = shaders[ShaderType::Mesh];
	}

	PsoID idPair{};
	idPair.GPassPsoID = CreatePSOIfNeeded(streamPtr, alignedStreamSize, false);

	// disable the pixel shader for depth prepass
	stream.ps = D3D12_SHADER_BYTECODE{};
	stream.depthStencil = d3dx::DepthState.REVERSED;
	idPair.DepthPsoID = CreatePSOIfNeeded(streamPtr, alignedStreamSize, true);

	return idPair;
}

} // anonymous namespace

namespace material {

id_t
CreateRootSignature(MaterialType::type materialType, ShaderFlags::flags shaderFlags)
{
	static_assert(sizeof(materialType) == sizeof(u32) && sizeof(shaderFlags) == sizeof(u32));
	const u64 key{ ((u64)materialType << 32) | shaderFlags };
	auto pair{ matRootSigMap.find(key) };
	if (pair != matRootSigMap.end())
	{
		return pair->second;
	}

	ID3D12RootSignature* rootSignature{ nullptr };
	switch (materialType)
	{
	case MaterialType::Opaque:
	{
		using params = gpass::OpaqueRootParameters;
		d3dx::D3D12RootParameter parameters[params::Count]{};
		parameters[params::GlobalShaderData].AsCBV(D3D12_SHADER_VISIBILITY_ALL, 0);

		D3D12_SHADER_VISIBILITY bufferVisibility{};
		D3D12_SHADER_VISIBILITY dataVisibility{};

		if (shaderFlags & ShaderFlags::Vertex)
		{
			bufferVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			dataVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		}
		else if (shaderFlags & ShaderFlags::Mesh)
		{
			bufferVisibility = D3D12_SHADER_VISIBILITY_MESH;
			dataVisibility = D3D12_SHADER_VISIBILITY_MESH;
		}

		if ((shaderFlags & ShaderFlags::Hull) || (shaderFlags & ShaderFlags::Geometry) || (shaderFlags & ShaderFlags::Amplification))
		{
			bufferVisibility = D3D12_SHADER_VISIBILITY_ALL;
			dataVisibility = D3D12_SHADER_VISIBILITY_ALL;
		}

		if ((shaderFlags & ShaderFlags::Pixel) || (shaderFlags & ShaderFlags::Compute))
		{
			dataVisibility = D3D12_SHADER_VISIBILITY_ALL;
		}

		parameters[params::PerObjectData].AsCBV(dataVisibility, 1);
		parameters[params::PositionBuffer].AsSRV(bufferVisibility, 0);
		parameters[params::ElementBuffer].AsSRV(bufferVisibility, 1);
		parameters[params::SrvIndices].AsSRV(D3D12_SHADER_VISIBILITY_PIXEL, 2); // TODO: needs to be visible to any stage that has to sample textures
		//TODO: lights

		const D3D12_STATIC_SAMPLER_DESC samplers[]
		{
			d3dx::StaticSampler(d3dx::SamplerState.STATIC_POINT, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL),
			d3dx::StaticSampler(d3dx::SamplerState.STATIC_LINEAR, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL),
			d3dx::StaticSampler(d3dx::SamplerState.STATIC_ANISOTROPIC, 2, 0, D3D12_SHADER_VISIBILITY_PIXEL),
		};

		rootSignature = d3dx::D3D12RootSignatureDesc
		{
			&parameters[0], params::Count, GetRootSignatureFlags(shaderFlags),
			&samplers[0], _countof(samplers)
		}.Create();
	}
	break;
	}

	assert(rootSignature);

	const id_t id{ (id_t)rootSignatures.size() };
	rootSignatures.emplace_back(rootSignature);
	matRootSigMap[key] = id;
	NAME_D3D12_OBJECT_INDEXED(rootSignature, key, L"GPass Root Signature - key:");

	return id;
}

void 
GetMaterials(const id_t* const materialIds, u32 materialCount, const MaterialsCache& cache, u32& outDescriptorIndexCount)
{
    assert(materialIds && materialCount);
    assert(cache.RootSignatures && cache.MaterialTypes);

    u32 totalIndexCount{ 0 };
    std::lock_guard lock{ materialMutex };
    for (u32 i{ 0 }; i < materialCount; ++i)
    {
        const D3D12MaterialStream stream{ materials[materialIds[i]].get() };
		cache.RootSignatures[i] = rootSignatures[stream.RootSignatureID()];
        cache.MaterialTypes[i] = stream.MaterialType();
        cache.DescriptorIndices[i] = stream.DescriptorIndices();
		cache.TextureCounts[i] = stream.TextureCount();
		cache.MaterialSurfaces[i] = stream.Surface();

        totalIndexCount += stream.TextureCount();
    }
    outDescriptorIndexCount = totalIndexCount;
}

id_t 
AddMaterial(const MaterialInitInfo& info)
{
	std::unique_ptr<u8[]> materialBuffer;
    std::lock_guard lock{ materialMutex };

	D3D12MaterialStream stream{ materialBuffer, info };

    assert(materialBuffer);
	return materials.add(std::move(materialBuffer));
}

void 
RemoveMaterial(id_t id)
{
    std::lock_guard lock{ materialMutex };
    materials.remove(id);
}
}

namespace render_item {

/*
* creates a buffer that is an array of id_t
* buffer[0] = geometryContentID
* buffer[1] = renderItemCount (the low-level render item ids, equal to the number of material ids/submeshes)
* buffer[2 .. n] = D3D12RenderItemIDs
*/
id_t 
AddRenderItem(id_t entityID, id_t geometryContentID, u32 materialCount, const id_t* const materialIDs)
{
	assert(id::IsValid(entityID) && id::IsValid(geometryContentID));
	assert(materialCount && materialIDs);

	// we need to create one render item for each of the submeshes of a geometry
	// the number of material ids must be the same as the number of submesh gpu ids
	id_t* const gpuIDs{ (id_t* const)alloca(materialCount * sizeof(id_t)) };
	mofu::content::GetSubmeshGpuIDs(geometryContentID, materialCount, gpuIDs);

	geometry::SubmeshViewsCache submeshViewsCache
	{
		(D3D12_GPU_VIRTUAL_ADDRESS* const)alloca(materialCount * sizeof(D3D12_GPU_VIRTUAL_ADDRESS)),
		(D3D12_GPU_VIRTUAL_ADDRESS* const)alloca(materialCount * sizeof(D3D12_GPU_VIRTUAL_ADDRESS)),
		(D3D12_INDEX_BUFFER_VIEW* const)alloca(materialCount * sizeof(D3D12_INDEX_BUFFER_VIEW)),
		(D3D_PRIMITIVE_TOPOLOGY* const)alloca(materialCount * sizeof(D3D_PRIMITIVE_TOPOLOGY)),
		(u32* const)alloca(materialCount * sizeof(u32)),
	};

	geometry::GetSubmeshViews(gpuIDs, materialCount, submeshViewsCache);
	// we need space for geometryContentID and renderItemCount
	std::unique_ptr<id_t[]> rItems{ std::make_unique<id_t[]>(sizeof(id_t) * (2 + (u64)materialCount)) };

	rItems[0] = geometryContentID;
	rItems[1] = materialCount;
	id_t* const rItemIDs{ &rItems[2] };
	D3D12RenderItem* const d3d12RenderItems{ (D3D12RenderItem*)alloca(materialCount * sizeof(D3D12RenderItem)) };

	for (u32 i{ 0 }; i < materialCount; ++i)
	{
		D3D12RenderItem& item{ d3d12RenderItems[i] };
		item.EntityID = entityID;
		item.SubmeshGpuID = gpuIDs[i];
		item.MaterialID = materialIDs[i];
		
		PsoID idPair{ CreatePSO(item.MaterialID, submeshViewsCache.PrimitiveTopologies[i], submeshViewsCache.ElementTypes[i]) };
		item.GPassPsoID = idPair.GPassPsoID;
		item.DepthPsoID = idPair.DepthPsoID;

		assert(id::IsValid(item.SubmeshGpuID) && id::IsValid(item.MaterialID));
	}

	std::lock_guard lock{ renderItemMutex };
	for (u32 i{ 0 }; i < materialCount; ++i)
	{
		rItemIDs[i] = renderItems.add(d3d12RenderItems[i]);
	}

	return renderItemIDs.add(std::move(rItems));
}

void 
RemoveRenderItem(id_t id)
{
	std::lock_guard lock{ renderItemMutex };
	const id_t* const buffer{ renderItemIDs[id].get() };
	const u32 renderItemCount{ buffer[1] };
	const id_t* const itemIDs{ &buffer[2] };
	for (u32 i{ 0 }; i < renderItemCount; ++i)
	{
		renderItems.remove(itemIDs[i]);
	}
	renderItemIDs.remove(id);
}

//NOTE: called at least once each frame
void 
GetRenderItemIds(const FrameInfo& frameInfo, Vec<id_t>& outIds)
{
	assert(frameInfo.RenderItemIDs && frameInfo.RenderItemCount && frameInfo.Thresholds);
	assert(!renderItemIDs.empty());

	FrameCache.LODOffsets.clear();
	FrameCache.GeometryIDs.clear();
	FrameCache.LODThresholds.clear();

	std::lock_guard lock{ renderItemMutex };
	const u32 count{ frameInfo.RenderItemCount };

	for (u32 i{ 0 }; i < count; ++i)
	{
		const id_t* const buffer{ renderItemIDs[frameInfo.RenderItemIDs[i]].get() };
		FrameCache.GeometryIDs.emplace_back(buffer[0]);
		FrameCache.LODThresholds.emplace_back(frameInfo.Thresholds[i]);
	}

	mofu::content::GetLODOffsets(FrameCache.GeometryIDs.data(), FrameCache.LODThresholds.data(), count, FrameCache.LODOffsets);
	assert(FrameCache.LODOffsets.size() == count);

	u32 renderItemCount{ 0 };
	for (u32 i{ 0 }; i < count; ++i)
	{
		renderItemCount += FrameCache.LODOffsets[i].Count;
	}
	assert(renderItemCount);
	outIds.resize(renderItemCount);

	// go through all render items for each geometry and only copy the render item IDs that belong to the selected LOD
	u32 itemIndex{ 0 };
	for (u32 i{ 0 }; i < count; ++i)
	{
		const id_t* const itemIDs{ &renderItemIDs[frameInfo.RenderItemIDs[i]][2] };
		const mofu::content::LodOffset& lodOffset{ FrameCache.LODOffsets[i] };
		memcpy(&outIds[itemIndex], &itemIDs[lodOffset.Offset], sizeof(id_t) * lodOffset.Count);
		itemIndex += lodOffset.Count;
		assert(itemIndex <= renderItemCount);
	}
	assert(itemIndex == renderItemCount);
}

void 
GetRenderItems(const id_t* const itemIDs, u32 idCount, const RenderItemsCache& cache)
{
	assert(itemIDs && idCount);
	assert(cache.EntityIDs && cache.SubmeshGpuIDs && cache.GpassPso && cache.DepthPso);

	std::lock_guard lock{ renderItemMutex };
	std::lock_guard psoLock{ psoMutex };
	for (u32 i{ 0 }; i < idCount; ++i)
	{
		const D3D12RenderItem& item{ renderItems[itemIDs[i]] };
		cache.EntityIDs[i] = item.EntityID;
		cache.SubmeshGpuIDs[i] = item.SubmeshGpuID;
		cache.MaterialIDS[i] = item.MaterialID;
		cache.GpassPso[i] = pipelineStates[item.GPassPsoID];
		cache.DepthPso[i] = pipelineStates[item.DepthPsoID];
	}
}
} // namespace render_item

}