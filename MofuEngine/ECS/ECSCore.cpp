#include "ECSCore.h"
#include "SystemRegistry.h"
#include "Scene.h"

#include "Systems/TransformSystem.cpp"
#include "Systems/SubmitEntityRenderSystem.cpp"

namespace mofu::ecs {
namespace {

} // anonymous namespace

void 
Initialize()
{
	scene::Initialize();
}

void 
Shutdown()
{
	scene::Shutdown();
}

void 
Update(system::SystemUpdateData data)
{
	system::SystemRegistry& systemRegistry{ system::SystemRegistry::Instance() };
	systemRegistry.UpdateSystems(system::SystemGroup::Initial, data);
	systemRegistry.UpdateSystems(system::SystemGroup::PreUpdate, data);
	systemRegistry.UpdateSystems(system::SystemGroup::Update, data);
	systemRegistry.UpdateSystems(system::SystemGroup::PostUpdate, data);
	systemRegistry.UpdateSystems(system::SystemGroup::Final, data);
}

}