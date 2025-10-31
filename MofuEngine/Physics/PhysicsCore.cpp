#include "PhysicsCore.h"
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <thread>
#include <stdarg.h>

#include "JobSystem.h"
#include "BroadPhaseLayerInterface.h"

#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"
#include "ECS/Transform.h"
#include "Utilities/Logger.h"

namespace mofu::physics::core {
namespace {

constexpr u32 TEMP_ALLOCATOR_SIZE{ 10 * 1024 * 1024 }; // 10MB

constexpr u32 MAX_RIGID_BODIES{ 8192 };
// how many mutexes to allocate to protect rigid bodies from concurrent access
constexpr u32 NUM_BODY_MUTEXES{ 0 }; // 0 - default jolt settings
// max amount of body pairs that can be queued at any time 
// (the broad phase will detect overlapping body pairs based on their bounding boxes and will insert them into a queue for the narrowphase)
constexpr u32 MAX_BODY_PAIRS{ 8192 };
// maximum size of the contact constraint buffer 
// if more contacts (collisions between bodies) are detected bodies will start interpenetrating / fall through the world
constexpr u32 MAX_CONTACT_CONSTRAINTS{ 2048 };

constexpr u32 COLLISION_STEPS{ 1 };

JPH::TempAllocatorImpl* _tempAllocator;
jobs::JobSystem* _jobSystem;

BroadPhaseLayerInterface _broadPhaseLayerInterface;
ObjectVsBroadPhaseLayerFilter _objectVsBroadphaseLayerFilter;
ObjectLayerPairFilter _objectVsObjectLayerFilter;
JPH::PhysicsSystem _physicsSystem;

//BodyActivationListener _bodyActivationListener;
//ContactListener _contactListener;

static void 
TraceImpl(const char* fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), fmt, list);
	va_end(list);

	DEBUG_LOG(buffer);
}

static bool 
AssertFailedImpl(const char* expression, const char* msg, const char* file, u32 line)
{
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "%s: %u: (%s) %s\n", file, line, expression, msg ? msg : "");
	DEBUG_LOG(buffer);
	assert(false);
	return true;
};

} // anonymous namespace

void
Initialize()
{
	JPH::RegisterDefaultAllocator();
	JPH::Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl);

	JPH::Factory::sInstance = new JPH::Factory();

	JPH::RegisterTypes();

	_tempAllocator = new JPH::TempAllocatorImpl{ TEMP_ALLOCATOR_SIZE };

	//const s32 jobSystemThreadCount{ (s32)(std::thread::hardware_concurrency() - 1u) };
	const s32 jobSystemThreadCount{ 1 };
	_jobSystem = new jobs::JobSystem{ JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, jobSystemThreadCount };

	_physicsSystem.Init(MAX_RIGID_BODIES, NUM_BODY_MUTEXES, MAX_BODY_PAIRS, MAX_CONTACT_CONSTRAINTS,
		_broadPhaseLayerInterface, _objectVsBroadphaseLayerFilter, _objectVsObjectLayerFilter);
}

void
FinalizePhysicsWorld()
{
	_physicsSystem.OptimizeBroadPhase();
}

void
Shutdown()
{
	JPH::UnregisterTypes();

	delete _jobSystem;

	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
}

void
Update(f32 deltaTime)
{
	_physicsSystem.Update(deltaTime, COLLISION_STEPS, _tempAllocator, _jobSystem);
	JPH::BodyInterface& bodyInterface{ _physicsSystem.GetBodyInterface() };

	for (auto [entity, d, lt, wt, collider] : ecs::scene::GetRW<ecs::component::DynamicObject, ecs::component::LocalTransform, ecs::component::WorldTransform, ecs::component::Collider>())
	{
		JPH::Vec3 pos;
		JPH::Quat rot;
		bodyInterface.GetPositionAndRotation(collider.BodyID, pos, rot);
		lt.Position = v3{ pos.GetX(), pos.GetY(), pos.GetZ() };
		lt.Rotation = quat{ rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW() };
		xmm q{ DirectX::XMLoadFloat4(&lt.Rotation) };
		xmm f{ DirectX::XMLoadFloat3(&v3forward) };
		xmm forwardV{ DirectX::XMVector3Rotate(f, q) };
		v3 forward{};
		DirectX::XMStoreFloat3(&forward, forwardV);
		lt.Forward = forward;
		//xmm posV{ DirectX::XMLoadFloat3((v3*)&pos)};
		//xmm rotV{ DirectX::XMLoadFloat4((v4*)&rot)};
		//v3 scale{ 1.f, 1.f, 1.f };
		//xmm scaleV{ DirectX::XMLoadFloat3(&scale) };
		//xmmat mat{ XMMatrixAffineTransformation(scaleV, DirectX::g_XMZero, rotV, posV)};
		//xmmat currentTransform{ DirectX::XMLoadFloat4x4(&wt.TRS) };
		//// TODO: apply at the right time
		//xmmat newTransform{ currentTransform * mat };
		//DirectX::XMStoreFloat4x4(&wt.TRS, newTransform);
		//log::Info("PHYSICS: Updated object transform");
	}

	// TODO: debug render
}

JPH::BodyInterface& BodyInterface() { return _physicsSystem.GetBodyInterface(); }

}