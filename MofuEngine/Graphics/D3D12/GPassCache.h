#pragma once
#include "Graphics/GraphicsTypes.h"
#include "D3D12Content.h"
#include "D3D12Content/D3D12Geometry.h"
#include "D3D12Content/D3D12Material.h"
#include "D3D12Content/D3D12Texture.h"
#include "ECS/Transform.h"
#include "EngineAPI/ECS/SceneAPI.h"

namespace mofu::graphics::d3d12::gpass {
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