#pragma once
#include "ECS/ECSCommon.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"

#include "ECS/Transform.h"
#include "Utilities/Logger.h"

#include "tracy/Tracy.hpp"

namespace mofu::graphics::d3d12 {
struct LightPostFrameSystem : ecs::system::System<LightPostFrameSystem>
{
	void Update([[maybe_unused]] const ecs::system::SystemUpdateData data)
	{
		ZoneScopedN("LightPostFrameSystem");

		//for (auto [entity, transform, light, updatedLight]
		//	: ecs::scene::GetRW<ecs::component::LocalTransform, ecs::component::Light, ecs::component::DirtyLight>())
		//{
		//}

		for (auto [entity, transform, light]
			: ecs::scene::GetRW<ecs::component::LocalTransform, ecs::component::Light>())
		{
		}

		// Update light buffers

	}
};
REGISTER_SYSTEM(LightPostFrameSystem, ecs::system::SystemGroup::Final, 3);

}