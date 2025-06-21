#pragma once
#include "ECS/ECSCommon.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"

#include "ECS/Transform.h"
#include "Utilities/Logger.h"
#include "Input/InputState.h"
#include "Input/InputSystem.h"

#define PRINT_DEBUG 1

namespace mofu::ecs::system {
	struct InputTestSystem : ecs::system::System<InputTestSystem>
	{
		void Update(const ecs::system::SystemUpdateData data)
		{
			input::Update();

			u8 isWDown{ input::IsKeyDown(input::Keys::W) };
			u8 wasWPressed{ input::WasKeyPressed(input::Keys::W) };
			u8 wasWReleased{ input::WasKeyReleased(input::Keys::W) };

#if PRINT_DEBUG
			log::Info("IsWDown? : %u", isWDown);
			log::Info("WasWPressed? : %u", wasWPressed);
			log::Info("WasWReleased? : %u", wasWReleased);
#endif
		}
	};
	REGISTER_SYSTEM(InputTestSystem, ecs::system::SystemGroup::PreUpdate, 0);
}