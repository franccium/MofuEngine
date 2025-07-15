#include "CommonHeaders.h"
#include "ECS/ECSCommon.h"
#include "Utilities/Logger.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"

#include "ECS/Transform.h"

using namespace mofu;

struct TestSystem : ecs::system::System<TestSystem>
{
	void Update([[maybe_unused]] const ecs::system::SystemUpdateData data)
	{
		//log::Info("TestSystem::Update");

		//for (auto [entity, transform] : ecs::scene::GetRW<ecs::component::LocalTransform>())
		//{
		//	log::Info("Found transform: Entity ID: %u, Position: (%f, %f, %f), Rotation: (%f, %f, %f), Scale: (%f, %f, %f)",
		//		entity.ID,
		//		transform.Position.x, transform.Position.y, transform.Position.z,
		//		transform.Rotation.x, transform.Rotation.y, transform.Rotation.z,
		//		transform.Scale.x, transform.Scale.y, transform.Scale.z);
		//	transform.Position.y += 0.01f * data.DeltaTime;
		//	transform.Position.x += 0.01f * data.DeltaTime;
		//}
	}
};
REGISTER_SYSTEM(TestSystem, ecs::system::SystemGroup::Update, 0);