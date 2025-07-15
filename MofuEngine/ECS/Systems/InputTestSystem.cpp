#pragma once
#include "ECS/ECSCommon.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"

#include "ECS/Transform.h"
#include "Utilities/Logger.h"
#include "Input/InputState.h"
#include "Input/InputSystem.h"

#define PRINT_DEBUG 0

namespace mofu::ecs::system {
	struct InputTestSystem : ecs::system::System<InputTestSystem>
	{
		void Update([[maybe_unused]] const ecs::system::SystemUpdateData data)
		{
			input::Update();
		}
	};
	REGISTER_SYSTEM(InputTestSystem, ecs::system::SystemGroup::PreUpdate, 0);
}
