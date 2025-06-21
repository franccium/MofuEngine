#pragma once
#include "ECS/ECSCommon.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"

#include "ECS/Transform.h"
#include "Utilities/Logger.h"
#include "EngineAPI/Camera.h"
#include "Input/InputSystem.h"

#define PRINT_DEBUG 1

namespace mofu::ecs::system {
	struct CameraFreeLookSystem : ecs::system::System<CameraFreeLookSystem>
	{
		void Update(const ecs::system::SystemUpdateData data)
		{
			for (auto [entity, lt, cam] : ecs::scene::GetRW<ecs::component::LocalTransform, ecs::component::Camera>())
			{
#if PRINT_DEBUG
				log::Info("Camera Position: (%f, %f, %f), Rotation: (%f, %f, %f) , Forward: %f %f %f",
					lt.Position.x, lt.Position.y, lt.Position.z,
					lt.Rotation.x, lt.Rotation.y, lt.Rotation.z,
					lt.Forward.x, lt.Forward.y, lt.Forward.z);
				//lt.Position.x += 0.002f * data.DeltaTime;
				//lt.Rotation.x -= 0.002f * data.DeltaTime;
#endif
				//graphics::Camera& cam{}

				v2 camSpeed{ 0.2f, 0.2f };
				f32 camRotSpeed{ 0.002f };
				if (input::IsKeyDown(input::Keys::W))
				{
					lt.Position.z += camSpeed.x * data.DeltaTime;
				}
				if (input::IsKeyDown(input::Keys::S))
				{
					lt.Position.z -= camSpeed.x * data.DeltaTime;
				}
				if (input::IsKeyDown(input::Keys::Q))
				{
					lt.Position.y += camSpeed.y * data.DeltaTime;
				}
				if (input::IsKeyDown(input::Keys::E))
				{
					lt.Position.y -= camSpeed.y * data.DeltaTime;
				}
				v2 delta{ input::GetMouseDelta() };
				lt.Rotation.x += delta.y;

				using namespace DirectX;
				xmm rot{ XMQuaternionRotationRollPitchYaw(lt.Rotation.x, lt.Rotation.y, lt.Rotation.z) };
				xmm dir{ 0.f, 0.f, 1.f, 0.f };
				XMStoreFloat3(&lt.Forward, XMVector3Normalize(XMVector3Rotate(dir, rot)));
			}
		}
	};
	REGISTER_SYSTEM(CameraFreeLookSystem, ecs::system::SystemGroup::Update, 0);

}