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
		void Update([[maybe_unused]] const ecs::system::SystemUpdateData data)
		{
			ZoneScopedN("PrepareEngineFrameInfo");
			graphics::FrameInfo frameInfo{};
			Vec<ecs::Entity>& visibleEntities{ graphics::GetVisibleEntities() };

			renderItemIDs.clear();
			thresholds.clear();
			visibleEntities.clear();

			frameInfo.LastFrameTime = 16.7f;
			frameInfo.AverageFrameTime = 16.7f;
			frameInfo.CameraID = camera_id{ 0 };

			for (auto [entity, transform, mesh]
				: ecs::scene::GetRW<ecs::component::WorldTransform,
					ecs::component::RenderMesh>())
			{
				renderItemIDs.emplace_back(mesh.RenderItemID);
				thresholds.emplace_back(0.f);
				visibleEntities.emplace_back(entity);
			}
			u32 renderItemCount = (u32)renderItemIDs.size();

			frameInfo.RenderItemCount = renderItemCount;
			frameInfo.RenderItemIDs = renderItemIDs.data();
			frameInfo.Thresholds = thresholds.data();

			graphics::SetCurrentFrameInfo(frameInfo);
		}
	};
	REGISTER_SYSTEM(PrepareEngineFrameInfo, ecs::system::SystemGroup::PreUpdate, 1);

}