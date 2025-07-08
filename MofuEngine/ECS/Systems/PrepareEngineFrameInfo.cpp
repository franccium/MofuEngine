#pragma once
#include "ECS/ECSCommon.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"

#include "Graphics/Renderer.h"

#include "ECS/Transform.h"
#include "Utilities/Logger.h"

#include "tracy/Tracy.hpp"

namespace mofu::graphics::d3d12 {
	Vec<f32> thresholds{};
	Vec<id_t> renderItemIDs{};

	struct PrepareEngineFrameInfo : ecs::system::System<PrepareEngineFrameInfo>
	{
		//TODO: figure out caching stuff and not updating unchanged
		void Update(const ecs::system::SystemUpdateData data)
		{
			ZoneScopedN("PrepareEngineFrameInfo");
			graphics::FrameInfo frameInfo{};
			
			if (renderItemIDs.empty())
			{
				renderItemIDs.clear();
				thresholds.clear();
			}
			
			u32 renderItemCount{ 0 };

			frameInfo.LastFrameTime = 16.7f;
			frameInfo.AverageFrameTime = 16.7f;
			frameInfo.CameraID = camera_id{ 0 };

			if (renderItemIDs.empty())
			{
				for (auto [entity, transform, mesh, material]
					: ecs::scene::GetRW<ecs::component::WorldTransform,
						ecs::component::RenderMesh, ecs::component::RenderMaterial>())
				{
					renderItemIDs.emplace_back(mesh.MeshID);
					thresholds.emplace_back(0.f);
					renderItemCount++;
				}
			}
			renderItemCount = renderItemIDs.size();

			frameInfo.RenderItemCount = renderItemCount;
			frameInfo.RenderItemIDs = renderItemIDs.data();
			frameInfo.Thresholds = thresholds.data();

			graphics::SetCurrentFrameInfo(frameInfo);
		}
	};
	REGISTER_SYSTEM(PrepareEngineFrameInfo, ecs::system::SystemGroup::PreUpdate, 0);

}