#include "ECSCore.h"
#include "SystemRegistry.h"
#include "Scene.h"

namespace mofu::graphics::d3d12 {
struct D3D12FrameInfo;
}

#include "Systems/TransformSystem.cpp"
#include "Systems/SubmitEntityRenderSystem.cpp"
#include "Systems/PrepareEngineFrameInfo.cpp"
//#include "Systems/TestCullingSystem.cpp"
#include "Systems/PrepareFrameRenderSystem.cpp"
#include "Systems/PreparePerObjectDataRenderSystem.cpp"
#include "Systems/InputTestSystem.cpp"
#include "Systems/CameraFreeLookSystem.cpp"



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
	//systemRegistry.UpdateSystems(system::SystemGroup::PostUpdate, data);
	//systemRegistry.UpdateSystems(system::SystemGroup::Final, data);
}

void UpdateRenderSystems(system::SystemUpdateData data, [[maybe_unused]] const graphics::d3d12::D3D12FrameInfo& d3d12FrameInfo)
{
	system::SystemRegistry& systemRegistry{ system::SystemRegistry::Instance() };
	systemRegistry.UpdateSystems(system::SystemGroup::PostUpdate, data);
	systemRegistry.UpdateSystems(system::SystemGroup::Final, data);
}

}