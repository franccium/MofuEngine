#include "CommonHeaders.h"
#include "ECS/ECSCommon.h"
#include "Utilities/Logger.h"
#include "EngineAPI/ECS/System.h"

using namespace mofu;

struct TestSystem : ecs::system::System<TestSystem>
{
	void Update(const ecs::system::SystemUpdateData data)
	{
		log::Info("TestSystem::Update");
	}
};
REGISTER_SYSTEM(TestSystem, ecs::system::SystemGroup::Update, 0);