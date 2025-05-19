#include "CommonHeaders.h"
#include "ECS/ECSCommon.h"
#include "Utilities/Logger.h"

using namespace mofu;

ADD_SYSTEM(TestSystem)
struct TestSystem : public ecs::system::System
{
	void Begin()
	{
		log::LogInfo("TestSystem::Begin");
	}

	void Update(f32 dt)
	{
		log::LogInfo("TestSystem::Update");
	}
};