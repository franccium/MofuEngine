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
	Vec<f32> thresholds2{};
	//Vec<ecs::Entity> visibleEntities{};
	Vec<ecs::Entity> lastVisibleEntities{};
	Vec<id_t> renderItemIDs2{};
	bool isFirst{ true };

	struct TestCullingSystem : ecs::system::System<TestCullingSystem>
	{
		//TODO: actual frustum culling
		void Update([[maybe_unused]] const ecs::system::SystemUpdateData data)
		{
			using namespace DirectX;
			Vec<ecs::Entity>& visibleEntities{ graphics::GetVisibleEntities() };
			ZoneScopedN("TestCullingSystem");
			graphics::FrameInfo frameInfo{};
			graphics::Camera mainCamera{ graphics::GetMainCamera() };

			renderItemIDs2.clear();
			thresholds2.clear();
			visibleEntities.clear();

			xmm camPos{};
			xmm camForward{};
			for (auto [entity, lt, cam] : ecs::scene::GetRW<ecs::component::LocalTransform, ecs::component::Camera>())
			{
				camPos = XMLoadFloat3(&lt.Position);
				camForward = XMLoadFloat3(&lt.Forward);
			}

			frameInfo.LastFrameTime = 16.7f;
			frameInfo.AverageFrameTime = 16.7f;
			frameInfo.CameraID = camera_id{ 0 };
			//v3 posa{ 0.f, 0.f, 20.f };
			//xmm ePos{ XMLoadFloat3(&posa) };

			//xmm dir{ ePos - camPos };
			//dir = XMVector3Normalize(dir);
			//xmm dot{ XMVector3Dot(camForward, dir) };
			//f32 dotf{};
			//XMStoreFloat(&dotf, dot);
			//if (dotf >= 0.f)
			//{

			//	log::Info("found %f", dotf);
			//}
			u32 id{ 0 };
			for (auto [entity, lt, wt, mesh]
				: ecs::scene::GetRW<ecs::component::LocalTransform, ecs::component::WorldTransform, ecs::component::RenderMesh>())
			{
				v3 pos{ wt.TRS._41, wt.TRS._42, wt.TRS._43 };
				xmm ePos{ XMLoadFloat3(&pos) };
				//xmm ePos{ XMLoadFloat3(&lt.Position) };
				xmm dir{ ePos - camPos };
				dir = XMVector3Normalize(dir);
				xmm dot{ XMVector3Dot(camForward, dir) };
				f32 dotf{};
				XMStoreFloat(&dotf, dot);
				if (dotf >= 0.f || isFirst)
				{
					renderItemIDs2.emplace_back(mesh.RenderItemID);
					thresholds2.emplace_back(0.f);
					visibleEntities.emplace_back(entity);

					//log::Info("found %f", dotf);
				}
				else
				{
					
				}
				id++;
			}
			isFirst = false;
			//if (visibleEntities.empty())
			//{
			//	renderItemIDs2.emplace_back(0);
			//	thresholds2.emplace_back(0.f);
			//	visibleEntities.emplace_back(1);
			//}
			u32 renderItemCount = (u32)renderItemIDs2.size();

			frameInfo.RenderItemCount = renderItemCount;
			frameInfo.RenderItemIDs = renderItemIDs2.data();
			frameInfo.Thresholds = thresholds2.data();

			log::Info("culled %u", id - visibleEntities.size());

			graphics::SetCurrentFrameInfo(frameInfo);
		}
	};
	REGISTER_SYSTEM(TestCullingSystem, ecs::system::SystemGroup::PreUpdate, 0);

}