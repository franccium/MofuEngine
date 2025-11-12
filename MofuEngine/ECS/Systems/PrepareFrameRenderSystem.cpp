#pragma once
#include "ECS/ECSCommon.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"
#include "ECS/SystemMessages.h"
#include "Input/InputSystem.h"

#include "Graphics/GraphicsTypes.h"
#include "Graphics/D3D12/D3D12Core.h"
#include "Graphics/D3D12/D3D12Content.h"
#include "Graphics/D3D12/D3D12Resources.h"
#include "Graphics/D3D12/D3D12GPass.h"
#include "Graphics/D3D12/D3D12Camera.h"
#include "Graphics/D3D12/D3D12Content/D3D12Geometry.h"
#include "Graphics/D3D12/D3D12Content/D3D12Material.h"
#include "Graphics/D3D12/D3D12Content/D3D12Texture.h"
#include "Graphics/D3D12/GPassCache.h"
#include "Graphics/D3D12/D3D12RayTracing.h"

#include "ECS/Transform.h"
#include "ECS/TransformHierarchy.h"
#include "Utilities/Logger.h"

#include "tracy/Tracy.hpp"

namespace mofu::graphics::d3d12 {
	struct PrepareFrameRenderSystem : ecs::system::System<PrepareFrameRenderSystem>
	{
		void FillPerObjectData(ecs::Entity entity, hlsl::PerObjectData* data, ecs::component::WorldTransform& transform,
			const MaterialSurface* const materialSurface, id_t materialID, const xmmat& cameraVP, const xmmat& cameraPrevVP)
		{
			using namespace DirectX;

			xmmat transformWorld{ XMLoadFloat4x4(&transform.TRS) };
			XMStoreFloat4x4(&data->World, transformWorld);
			transformWorld.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);
			xmmat inverseWorld{ XMMatrixInverse(nullptr, transformWorld) };
			XMStoreFloat4x4(&data->InvWorld, inverseWorld);

			xmmat world{ XMLoadFloat4x4(&data->World) };
			xmmat wvp{ XMMatrixMultiply(world, cameraVP) };
			XMStoreFloat4x4(&data->WorldViewProjection, wvp);
#if IS_DLSS_ENABLED
			xmmat prevWorld{ XMLoadFloat4x4(ecs::transform::GetPreviousTransform(entity)) };
			xmmat prevWVP{ XMMatrixMultiply(prevWorld, cameraPrevVP) };
			XMStoreFloat4x4(&data->PrevWorldViewProjection, prevWVP);
#endif

			memcpy(&data->BaseColor, materialSurface, sizeof(MaterialSurface));
			data->MaterialID = (u16)materialID;
		}


		//TODO: figure out caching stuff and not updating unchanged
		void Update([[maybe_unused]] const ecs::system::SystemUpdateData data)
		{
			ZoneScopedN("PrepareFrameRenderSystem");

			ConstantBuffer& cbuffer{ core::CBuffer() };
			gpass::GPassCache& frameCache{ gpass::GetGPassFrameCache() };
			D3D12FrameInfo& frameInfo{ gpass::GetCurrentD3D12FrameInfo() };

			using namespace content;
			render_item::GetRenderItemIds(*frameInfo.Info, frameCache.D3D12RenderItemIDs);
			frameCache.Resize();
			const u32 renderItemCount{ frameCache.Size() };

			const render_item::RenderItemsCache renderItemCache{ frameCache.GetRenderItemsCache() };
			render_item::GetRenderItems(frameCache.D3D12RenderItemIDs.data(), renderItemCount, renderItemCache);

			const geometry::SubmeshViewsCache submeshViewCache{ frameCache.GetSubmeshViewsCache() };
			geometry::GetSubmeshViews(frameCache.SubmeshGpuIDs, renderItemCount, submeshViewCache);

			u32 renderItemIndex{ 0 };

			Vec<ecs::Entity>& visible{ graphics::GetVisibleEntities() };
			for (auto e : visible)
			{
				auto material{ ecs::scene::GetComponent<ecs::component::RenderMaterial>(e) };
				if (renderItemIndex < renderItemCount)
				{
					frameCache.MaterialIDs[renderItemIndex] = material.MaterialID;
					renderItemIndex++;
				}
			}

			/*for (auto [entity, material, vis]
				: ecs::scene::GetRW<ecs::component::RenderMaterial, ecs::component::PotentiallyVisible>())
			{
				if (renderItemIndex < renderItemCount)
				{
					renderItemIndex = id::Index(entity) - 1;
					frameCache.MaterialIDs[renderItemIndex] = material.MaterialID;
				}
			}*/

			const material::MaterialsCache materialsCache{ frameCache.GetMaterialsCache() };
			material::GetMaterials(frameCache.MaterialIDs, renderItemCount, materialsCache, frameCache.DescriptorIndexCount);

			hlsl::PerObjectData* currentDataPtr{ nullptr };

			renderItemIndex = 0;
			const xmmat camVP{ frameInfo.Camera->ViewProjection() };
			const m4x4 prevVP{ frameInfo.Camera->PrevViewProjection() };
			const xmmat camPrevVP{ DirectX::XMLoadFloat4x4(&prevVP) };
			for (auto e : visible)
			{
				auto& wt{ ecs::scene::GetComponent<ecs::component::WorldTransform>(e) };
				if (renderItemIndex < renderItemCount)
				{
					currentDataPtr = cbuffer.AllocateSpace<hlsl::PerObjectData>();
					//FillPerObjectData(data, transform, *material.MaterialSurface, cameraVP);
					FillPerObjectData(e, currentDataPtr, wt, materialsCache.MaterialSurfaces[renderItemIndex], 
						frameCache.MaterialIDs[renderItemIndex], camVP, camPrevVP);
					//}
					assert(currentDataPtr);
					frameCache.PerObjectData[renderItemIndex] = cbuffer.GpuAddress(currentDataPtr);
					renderItemIndex++;
				}
				else
				{
					log::Warn("renderItemIndex > renderItemCount");
				}
			}

			//for (auto [entity, transform, mesh, material, vis] 
			//	: ecs::scene::GetRW<ecs::component::WorldTransform, 
			//		ecs::component::RenderMesh, ecs::component::RenderMaterial, ecs::component::PotentiallyVisible>())
			//{
			//	//if (entity != frameCache.EntityIDs[renderItemIndex])
			//	//{
			//		// PER OBJECT DATA
			//	renderItemIndex = id::Index(entity) - 1; //FIXME: make this make sense, placeholder solution now that assumes first entity is camera and the rest are all renderable render items; cant just renderItemIndex cause first block contains entities 1 and 9, so the parent entities lose their render items
			//	if (renderItemIndex < renderItemCount)
			//	{
			//		currentDataPtr = cbuffer.AllocateSpace<hlsl::PerObjectData>();
			//		//FillPerObjectData(data, transform, *material.MaterialSurface, cameraVP);
			//		FillPerObjectData(currentDataPtr, transform, materialsCache.MaterialSurfaces[renderItemIndex], frameInfo.Camera->ViewProjection());
			//		//}
			//		assert(currentDataPtr);
			//		frameCache.PerObjectData[renderItemIndex] = cbuffer.GpuAddress(currentDataPtr);
			//	}
			//	else
			//	{
			//		log::Warn("renderItemIndex > renderItemCount");
			//	}
			//}

			// TEXTURES
			if (frameCache.DescriptorIndexCount != 0)
			{
				const u32 size{ frameCache.DescriptorIndexCount * sizeof(u32) };
				u32* const srvIndices{ (u32* const)cbuffer.AllocateSpace(size) };
				u32 srvIndexOffset{ 0 };

				for (u32 i{ 0 }; i < renderItemCount; ++i)
				{
					const u32 textureCount{ frameCache.TextureCounts[i] };
					frameCache.SrvIndices[i] = 0;

					if (textureCount != 0)
					{
						const u32* const indices{ frameCache.DescriptorIndices[i] };
						memcpy(&srvIndices[srvIndexOffset], indices, textureCount * sizeof(u32));
						frameCache.SrvIndices[i] = cbuffer.GpuAddress(srvIndices + srvIndexOffset);

						srvIndexOffset += textureCount;
					}
				}
			}

#if RAYTRACING
			if (ecs::messages::GetBoolMessage(ecs::messages::SystemBoolMessage::TransformChanged) 
				|| input::WasKeyPressed(input::Keybinds::Debug.RTASRebuild))
				graphics::d3d12::rt::UpdateAccelerationStructure();
#endif

			//TODO: issue commands

			assert(frameCache.IsValid());
		}
	};
	REGISTER_SYSTEM(PrepareFrameRenderSystem, ecs::system::SystemGroup::PostUpdate, 0);

}