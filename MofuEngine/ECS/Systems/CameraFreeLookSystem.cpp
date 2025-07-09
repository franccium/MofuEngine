#pragma once
#include "ECS/ECSCommon.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"

#include "ECS/Transform.h"
#include "Utilities/Logger.h"
#include "EngineAPI/Camera.h"
#include "Input/InputSystem.h"

#define PRINT_DEBUG 0

namespace mofu::ecs::system {
	bool isInputEnabled{ true };
	
	struct CameraFreeLookSystem : ecs::system::System<CameraFreeLookSystem>
	{
		void Update(const ecs::system::SystemUpdateData data)
		{
			for (auto [entity, lt, cam] : ecs::scene::GetRW<ecs::component::LocalTransform, ecs::component::Camera>())
			{
				//graphics::Camera& cam{}
				using namespace DirectX;

				v3 direction{ lt.Forward };
				f32 theta{ XMScalarACos(direction.y) };
				f32 phi{ std::atan2(-direction.z, direction.x) };
				v3 rot{ theta - math::HALF_PI, phi + math::HALF_PI, 0.f };
				v3 spherical;
				v3 move{};
				spherical = rot;
				cam.TargetPos = lt.Position;
				
				if (input::IsKeyDown(input::Keys::R))
				{
					isInputEnabled = !isInputEnabled;
				}
				if (input::IsKeyDown(input::Keys::W))
				{
					move.z = 1.f;
				}
				if (input::IsKeyDown(input::Keys::S))
				{
					move.z = -1.f;
				}
				if (input::IsKeyDown(input::Keys::A))
				{
					move.x = 1.f;
				}
				if (input::IsKeyDown(input::Keys::D))
				{
					move.x = -1.f;
				}
				if (input::IsKeyDown(input::Keys::Q))
				{
					move.y = 1.f;
				}
				if (input::IsKeyDown(input::Keys::E))
				{
					move.y = -1.f;
				}
				xmm moveV{ XMLoadFloat3(&move) };
				f32 moveMagnitude{ XMVectorGetX(XMVector3LengthSq(moveV)) };
				/*if (input::IsKeyDown(input::Keys::W))
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
				}*/
				if (isInputEnabled)
				{
					v2 delta{ input::GetMouseDelta() };
					if (!math::IsEqual(delta.x, 0.f) || !math::IsEqual(delta.y, 0.f))
					{
						f32 rotSpeed{ 1.5f };
						spherical.y -= delta.x * rotSpeed;
						spherical.x += delta.y * rotSpeed;
						spherical.x = math::Clamp(spherical.x, 0.0001f - math::HALF_PI, math::HALF_PI - 0.0001);

						cam.TargetRot = spherical;
					}
					//lt.Rotation.x += delta.y;

					//using namespace DirectX;
					//xmm rot{ XMLoadFloat4(&lt.Rotation) };
					//xmm dir{ 0.f, 0.f, 1.f, 0.f };
					//XMStoreFloat3(&lt.Forward, XMVector3Normalize(XMVector3Rotate(dir, rot)));
				}

				f32 slerpFactor{ 0.5f };
				xmm rotV{ XMLoadFloat4(&lt.Rotation) };
				xmm targetRot{ XMLoadFloat3(&cam.TargetRot) };
				xmm targetRotQ{ XMQuaternionRotationRollPitchYawFromVector(targetRot) };
				xmm rotD{ targetRotQ - rotV };

				xmm newRot{ XMQuaternionSlerp(rotV, targetRotQ, slerpFactor) };
				XMStoreFloat4(&lt.Rotation, newRot);

				xmm dir{ 0.f, 0.f, 1.f, 0.f };
				XMStoreFloat3(&lt.Forward, XMVector3Normalize(XMVector3Rotate(dir, newRot)));

#if PRINT_DEBUG
				log::Info("Camera forward: %f, %f, %f", lt.Forward.x, lt.Forward.y, lt.Forward.z);
				log::Info("Camera rot: %f, %f, %f, %f", lt.Rotation.w, lt.Rotation.x, lt.Rotation.y, lt.Rotation.z);
				log::Info("Camera targetRot: %f, %f, %f", cam.TargetRot.x, cam.TargetRot.y, cam.TargetRot.z);
#endif

				const f32 fpsScale{ data.DeltaTime / 0.016667f };
				if (moveMagnitude > math::EPSILON)
				{
					v4 rot{ lt.Rotation };
					f32 moveSpeed{ 0.002f };
					xmm dir{ XMVector3Rotate(moveV * moveSpeed * fpsScale, XMLoadFloat4(&rot)) };
					xmm target{ XMLoadFloat3(&cam.TargetPos) };
					target += (dir * moveSpeed);
					XMStoreFloat3(&cam.TargetPos, target);

					xmm posV{ XMLoadFloat3(&lt.Position) };

					xmm posD{ target - posV };
					posV += posD * fpsScale;

					XMStoreFloat3(&lt.Position, posV);
				}
			}
		}
	};
	REGISTER_SYSTEM(CameraFreeLookSystem, ecs::system::SystemGroup::Update, 2);

}