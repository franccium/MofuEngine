#pragma once
#include "ECS/ECSCommon.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"

#include "ECS/Transform.h"
#include "Utilities/Logger.h"

#define PRINT_DEBUG 0

namespace mofu::ecs::system {
struct TransformSystem : ecs::system::System<TransformSystem>
{
	void Update(const ecs::system::SystemUpdateData data)
	{
		log::Info("TransformSystem::Update");

		for (auto [entity, lt, wt] : ecs::scene::GetRW<ecs::component::LocalTransform, ecs::component::WorldTransform>())
		{
#if PRINT_DEBUG
			log::Info("Found lt: Entity ID: %u, Position: (%f, %f, %f), Rotation: (%f, %f, %f), Scale: (%f, %f, %f)",
				entity,
				lt.Position.x, lt.Position.y, lt.Position.z,
				lt.Rotation.x, lt.Rotation.y, lt.Rotation.z,
				lt.Scale.x, lt.Scale.y, lt.Scale.z);
			lt.Position.y += 0.0001f * data.DeltaTime;
			lt.Position.x += 0.0001f * data.DeltaTime;
#endif

			using namespace DirectX;
			xmm scale = XMLoadFloat3(&lt.Scale);
			xmm rot = XMQuaternionRotationRollPitchYaw(lt.Rotation.x, lt.Rotation.y, lt.Rotation.z);
			xmm pos = XMLoadFloat3(&lt.Position);
			XMMATRIX trs = DirectX::XMMatrixAffineTransformation(scale, g_XMZero, rot, pos);
			XMStoreFloat4x4(&wt.TRS, trs);
#if PRINT_DEBUG
			log::Info("Modified TRS: Entity ID: %u, TRS: T:(%f, %f, %f, ...)",
				entity,
				wt.TRS._41, wt.TRS._42, wt.TRS._43);
#endif
		}
	}
};
REGISTER_SYSTEM(TransformSystem, ecs::system::SystemGroup::Update, 0);

}