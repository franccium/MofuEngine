#pragma once
#include "ECS/ECSCommon.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"

#include "ECS/Transform.h"
#include "Utilities/Logger.h"

#include "tracy/Tracy.hpp"

namespace mofu::graphics::d3d12 {
	struct LightPrepareRenderSystem : ecs::system::System<LightPrepareRenderSystem>
	{
		void Update(const ecs::system::SystemUpdateData data)
		{
			ZoneScopedN("LightPrepareRenderSystem");

			
		}
	};
	REGISTER_SYSTEM(LightPrepareRenderSystem, ecs::system::SystemGroup::PreUpdate, 3);
}