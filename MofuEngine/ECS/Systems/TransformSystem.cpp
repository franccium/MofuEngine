#pragma once
#include "ECS/ECSCommon.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"

#include "ECS/Transform.h"
#include "Utilities/Logger.h"
#include "ECS/TransformHierarchy.h"

#include "tracy/Tracy.hpp"

#define PRINT_DEBUG 0
#define PRINT_DEBUG_PARENT 0

namespace mofu::ecs::system {

struct TransformSystem : ecs::system::System<TransformSystem>
{
	void Update([[maybe_unused]] const ecs::system::SystemUpdateData data)
	{
		ZoneScopedN("TransformSystem");

		ecs::transform::UpdateHierarchy(); //TODO: move this somewhere
	}
};
REGISTER_SYSTEM(TransformSystem, ecs::system::SystemGroup::Update, 0);

}