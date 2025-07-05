#pragma once
#include "Graphics/GraphicsTypes.h"
#include "D3D12Content.h"
#include "D3D12Content/D3D12Geometry.h"
#include "D3D12Content/D3D12Material.h"
#include "D3D12Content/D3D12Texture.h"
#include "ECS/Transform.h"
#include "EngineAPI/ECS/SceneAPI.h"

namespace mofu::graphics::d3d12::gpass {
	
inline void ValidateGpuAddress(D3D12_GPU_VIRTUAL_ADDRESS address)
{
	if (address == 0 || address == ~0ull)
	{
		__debugbreak();
		throw std::runtime_error("Invalid GPU address");
	}
}

inline void ValidatePipelineState(ID3D12PipelineState* pso)
{
	if (pso == nullptr)
	{
		__debugbreak();
		throw std::runtime_error("Null PSO");
	}

	pso->AddRef();
	pso->Release();
}

struct GPassCache
{
	// NOTE: when adding new arrays, make sure to update Resize() and StructSize
	Vec<id_t> D3D12RenderItemIDs{};
	u32 DescriptorIndexCount{ 0 };

	// Render Items Cache
	ecs::Entity* EntityIDs{ nullptr };
	id_t* SubmeshGpuIDs{ nullptr };
	id_t* MaterialIDs{ nullptr };
	ID3D12PipelineState** GPassPipelineStates{ nullptr };
	ID3D12PipelineState** DepthPipelineStates{ nullptr };

	// Materials Cache	
	ID3D12RootSignature** RootSignatures{ nullptr };
	MaterialType::type* MaterialTypes{ nullptr };
	u32** DescriptorIndices{ nullptr };
	u32* TextureCounts{ nullptr };
	MaterialSurface** MaterialSurfaces{ nullptr };

	// Submesh Views Cache
	D3D12_GPU_VIRTUAL_ADDRESS* PositionBuffers{ nullptr };
	D3D12_GPU_VIRTUAL_ADDRESS* ElementBuffers{ nullptr };
	D3D12_INDEX_BUFFER_VIEW* IndexBufferViews{ nullptr };
	D3D_PRIMITIVE_TOPOLOGY* PrimitiveTopologies{ nullptr };
	u32* ElementTypes{ nullptr };

	D3D12_GPU_VIRTUAL_ADDRESS* PerObjectData{ nullptr };
	D3D12_GPU_VIRTUAL_ADDRESS* SrvIndices{ nullptr };

	bool IsValid() const
	{
		const u32 count = D3D12RenderItemIDs.size();

		for (u32 i{ 0 }; i < count; ++i)
		{
			ValidateGpuAddress(PerObjectData[i]);
			if(TextureCounts[i] != 0)
				ValidateGpuAddress(SrvIndices[i]);
			ValidateGpuAddress(ElementBuffers[i]);
			ValidateGpuAddress(PositionBuffers[i]);
			ValidatePipelineState(GPassPipelineStates[i]);
			ValidatePipelineState(DepthPipelineStates[i]);
		}


		// Check all arrays are either null or have the correct size
#define CHECK_ARRAY(ptr) \
            if (ptr != nullptr && count == 0) { \
                /* Array is allocated but count is zero */ \
                assert(false && #ptr " is allocated but count is zero"); \
                return false; \
            } \
            if (ptr == nullptr && count > 0) { \
                /* Array is null but count suggests it should be allocated */ \
                assert(false && #ptr " is null but count suggests it should be allocated"); \
                return false; \
            }

		// Render Items Cache
		CHECK_ARRAY(EntityIDs);
		CHECK_ARRAY(SubmeshGpuIDs);
		CHECK_ARRAY(MaterialIDs);
		CHECK_ARRAY(GPassPipelineStates);
		CHECK_ARRAY(DepthPipelineStates);

		// Materials Cache
		CHECK_ARRAY(RootSignatures);
		CHECK_ARRAY(MaterialTypes);
		CHECK_ARRAY(DescriptorIndices);
		CHECK_ARRAY(TextureCounts);
		CHECK_ARRAY(MaterialSurfaces);

		// Submesh Views Cache
		CHECK_ARRAY(PositionBuffers);
		CHECK_ARRAY(ElementBuffers);
		CHECK_ARRAY(IndexBufferViews);
		CHECK_ARRAY(PrimitiveTopologies);
		CHECK_ARRAY(ElementTypes);

		// Per-object data
		CHECK_ARRAY(PerObjectData);
		CHECK_ARRAY(SrvIndices);

#undef CHECK_ARRAY

		return true;
	}

	constexpr content::render_item::RenderItemsCache GetRenderItemsCache() const
	{
		return
		{
			EntityIDs,
			SubmeshGpuIDs,
			MaterialIDs,
			GPassPipelineStates,
			DepthPipelineStates,
		};
	}

	constexpr content::geometry::SubmeshViewsCache GetSubmeshViewsCache() const
	{
		return
		{
			PositionBuffers,
			ElementBuffers,
			IndexBufferViews,
			PrimitiveTopologies,
			ElementTypes,
		};
	}

	constexpr content::material::MaterialsCache	GetMaterialsCache() const
	{
		return
		{
			RootSignatures,
			MaterialTypes,
			DescriptorIndices,
			TextureCounts,
			MaterialSurfaces,
		};
	}

	void Clear()
	{
		D3D12RenderItemIDs.clear();
		DescriptorIndexCount = 0;
	}

	constexpr u32 Size() const { return (u32)D3D12RenderItemIDs.size(); }

	constexpr void Resize()
	{
		const u64 RenderItemCount{ D3D12RenderItemIDs.size() };
		const u64 OldBufferSize{ _buffer.size() };
		const u64 NewBufferSize{ RenderItemCount * STRUCT_SIZE };

		if (NewBufferSize != OldBufferSize)
		{
			if (NewBufferSize > OldBufferSize)
			{
				_buffer.resize(NewBufferSize);
			}

			EntityIDs = (ecs::Entity*)_buffer.data();
			SubmeshGpuIDs = (id_t*)(&EntityIDs[RenderItemCount]);
			MaterialIDs = (id_t*)(&SubmeshGpuIDs[RenderItemCount]);
			GPassPipelineStates = (ID3D12PipelineState**)(&MaterialIDs[RenderItemCount]);
			DepthPipelineStates = (ID3D12PipelineState**)(&GPassPipelineStates[RenderItemCount]);
			RootSignatures = (ID3D12RootSignature**)(&DepthPipelineStates[RenderItemCount]);
			MaterialTypes = (MaterialType::type*)(&RootSignatures[RenderItemCount]);
			DescriptorIndices = (u32**)(&MaterialTypes[RenderItemCount]);
			TextureCounts = (u32*)(&DescriptorIndices[RenderItemCount]);
			MaterialSurfaces = (MaterialSurface**)(&TextureCounts[RenderItemCount]);
			PositionBuffers = (D3D12_GPU_VIRTUAL_ADDRESS*)(&MaterialSurfaces[RenderItemCount]);
			ElementBuffers = (D3D12_GPU_VIRTUAL_ADDRESS*)(&PositionBuffers[RenderItemCount]);
			IndexBufferViews = (D3D12_INDEX_BUFFER_VIEW*)(&ElementBuffers[RenderItemCount]);
			PrimitiveTopologies = (D3D_PRIMITIVE_TOPOLOGY*)(&IndexBufferViews[RenderItemCount]);
			ElementTypes = (u32*)(&PrimitiveTopologies[RenderItemCount]);
			PerObjectData = (D3D12_GPU_VIRTUAL_ADDRESS*)(&ElementTypes[RenderItemCount]);
			SrvIndices = (D3D12_GPU_VIRTUAL_ADDRESS*)(&PerObjectData[RenderItemCount]);
		}
	}

private:
	constexpr static u32 STRUCT_SIZE
	{
		sizeof(id_t) + sizeof(id_t) + sizeof(id_t) +
		sizeof(ID3D12PipelineState*) + sizeof(ID3D12PipelineState*) +
		sizeof(ID3D12RootSignature*) +
		sizeof(MaterialType::type) +
		sizeof(u32*) +
		sizeof(u32) +
		sizeof(MaterialSurface*) +
		sizeof(D3D12_GPU_VIRTUAL_ADDRESS) + sizeof(D3D12_GPU_VIRTUAL_ADDRESS) +
		sizeof(D3D12_INDEX_BUFFER_VIEW) + sizeof(D3D_PRIMITIVE_TOPOLOGY) +
		sizeof(u32) + sizeof(D3D12_GPU_VIRTUAL_ADDRESS) +
		sizeof(D3D12_GPU_VIRTUAL_ADDRESS)
	};

	Vec<u8> _buffer;
};
}