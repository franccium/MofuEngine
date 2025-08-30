#pragma once
#include "ECS/ECSCommon.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"

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

#include "ECS/Transform.h"
#include "Utilities/Logger.h"

#include "tracy/Tracy.hpp"

namespace mofu::graphics::d3d12 {
	struct FrustumCullingSystem : ecs::system::System<FrustumCullingSystem>
	{
		//TODO: figure out caching stuff and not updating unchanged
		void Update([[maybe_unused]] const ecs::system::SystemUpdateData data)
		{
			ZoneScopedN("FrustumCullingSystem");
			graphics::FrameInfo frameInfo{};
			graphics::Camera mainCamera{ graphics::GetMainCamera() };
			Vec<ecs::Entity>& visibleEntities{ graphics::GetVisibleEntities() };
			using namespace DirectX;

			visibleEntities.clear();

			frameInfo.LastFrameTime = 16.7f;
			frameInfo.AverageFrameTime = 16.7f;
			frameInfo.CameraID = camera_id{ 0 };

			for (auto [entity, transform, mesh]
				: ecs::scene::GetRW<ecs::component::WorldTransform,
					ecs::component::CullableObject>())
			{
			}
			
		}
	};
	REGISTER_SYSTEM(FrustumCullingSystem, ecs::system::SystemGroup::PreUpdate, 1);

}