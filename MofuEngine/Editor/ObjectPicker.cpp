#include "ObjectPicker.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/AABoxCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include "Physics/PhysicsCore.h"
#include "Physics/PhysicsLayers.h"
#include "Physics/DebugRenderer/DebugRenderer.h"
#include "Input/InputSystem.h"
#include "ECS/Scene.h"
#include "EngineAPI/ECS/SceneAPI.h"

namespace mofu::editor::object {
namespace {
ecs::Entity _pickedEntity{ U32_INVALID_ID };
JPH::BodyID _pickedBody{};
ecs::Entity _cameraEntity{ U32_INVALID_ID };

bool
CastProbe(f32 probeLength, f32& outFraction, JPH::RVec3& outPos, JPH::BodyID& outBodyID)
{
	if(!ecs::scene::IsEntityAlive(_cameraEntity))
		_cameraEntity = ecs::scene::GetSingletonEntity(ecs::component::ID<ecs::component::Camera>);

	const ecs::component::LocalTransform& camLT{ ecs::scene::GetComponent<ecs::component::LocalTransform>(_cameraEntity) };

	JPH::RVec3 origin{ camLT.Position.AsJPVec3() };
	JPH::Vec3 direction{ (camLT.Forward * probeLength).AsJPVec3() };

	outPos = origin + direction;
	outFraction = 1.0f;
	outBodyID = JPH::BodyID{};
	bool hit{ false };

	JPH::RRayCast ray{ origin, direction };
	JPH::RayCastResult res;
	//hit = physics::core::PhysicsSystem().GetNarrowPhaseQuery().CastRay(ray, res,
		//JPH::SpecifiedBroadPhaseLayerFilter{ physics::bphLayers::Static }, JPH::SpecifiedObjectLayerFilter{ physics::PhysicsLayers::Static });
	hit = physics::core::PhysicsSystem().GetNarrowPhaseQuery().CastRay(ray, res);
	outPos = ray.GetPointOnRay(res.mFraction);
	outFraction = res.mFraction;
	outBodyID = res.mBodyID;
	if (hit)
	{
		graphics::d3d12::debug::GetDebugRenderer()->DrawMarker(outPos, JPH::Color::sYellow, 0.1f);
		JPH::BodyLockRead lock{ physics::core::PhysicsSystem().GetBodyLockInterface(), outBodyID};
		_pickedEntity = (ecs::Entity)lock.GetBody().GetUserData();
	}
	else
	{
		graphics::d3d12::debug::GetDebugRenderer()->DrawMarker((camLT.Position + (camLT.Forward * 0.1f)).AsJPVec3(), JPH::Color::sRed, 0.001f);
	}
	return hit;
}

} // anonymous namespace

void 
UpdateObjectPickerProbe()
{
	if (input::WasKeyPressed(input::Keybinds::Editor.PickObject))
	{
		JPH::RVec3 hitPos{};
		JPH::BodyID body{};
		f32 fraction{};
		f32 probeLength{ 100.f };
		if (CastProbe(probeLength, fraction, hitPos, body))
		{
			log::Info("Hit Body: %u", body.GetIndex());
		}
	}

	if (ecs::scene::IsEntityAlive(_pickedEntity))
	{
		const ecs::component::LocalTransform& lt{ ecs::scene::GetEntityComponent<ecs::component::LocalTransform>(_pickedEntity) };
		ecs::component::Collider col{ ecs::scene::GetEntityComponent<ecs::component::Collider>(_pickedEntity) };
		auto shape{ physics::core::PhysicsSystem().GetBodyInterface().GetShape(col.BodyID).GetPtr() };
		JPH::AABox box{ shape->GetLocalBounds() };
		JPH::Color color{ JPH::Color{ JPH::Color::sRed } };
		graphics::d3d12::debug::GetDebugRenderer()->DrawWireBox(
			JPH::RMat44::sRotationTranslation(lt.Rotation, lt.Position.AsJPVec3()) * JPH::Mat44::sScale(lt.Scale.AsJPVec3()),
			shape->GetLocalBounds(), color);
	}
}

void Initialize()
{
}

ecs::Entity GetPickedEntity() { return _pickedEntity; }

}