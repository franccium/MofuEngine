#pragma once
#include "ECS/ECSCommon.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"

#include "ECS/Transform.h"
#include "Utilities/Logger.h"

namespace mofu::ecs::system {
	struct SubmitEntityRenderSystem : ecs::system::System<SubmitEntityRenderSystem>
	{
		void Update([[maybe_unused]] const ecs::system::SystemUpdateData data)
		{
			//log::Info("SubmitEntityRenderSystem::Update");

			//for (auto [entity, wt, renderable] : ecs::scene::GetRW<ecs::component::WorldTransform, ecs::component::RenderMesh>())
			//{
			//	
			//}
		}
	};
	REGISTER_SYSTEM(SubmitEntityRenderSystem, ecs::system::SystemGroup::PostUpdate, 2);

}