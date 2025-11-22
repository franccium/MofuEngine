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
#include "SceneEditorView.h"

namespace mofu::editor::object {
namespace {
ecs::Entity _pickedEntity{ U32_INVALID_ID };
JPH::BodyID _pickedBody{};
ecs::Entity _cameraEntity{ U32_INVALID_ID };

bool
CastProbe(f32 probeLength, f32& outFraction, JPH::RVec3& outPos, JPH::BodyID& outBodyID, bool interacted)
{
	if(!ecs::scene::IsEntityAlive(_cameraEntity))
		_cameraEntity = ecs::scene::GetSingletonEntity(ecs::component::ID<ecs::component::Camera>);

	const ecs::component::LocalTransform& camLT{ ecs::scene::GetComponent<ecs::component::LocalTransform>(_cameraEntity) };

	JPH::RVec3 origin{ camLT.Position.Vec3() };
	JPH::Vec3 direction{ (camLT.Forward * probeLength).Vec3() };

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

		auto renderer{ graphics::d3d12::debug::GetDebugRenderer() };
		//renderer->DrawMarker(outPos, JPH::Color::sYellow, 0.1f);
		JPH::BodyLockRead lock{ physics::core::PhysicsSystem().GetBodyLockInterface(), outBodyID};
		if (lock.Succeeded())
		{
			const JPH::Body& hitBody{ lock.GetBody() };
			if (interacted)
			{
				_pickedEntity = (ecs::Entity)hitBody.GetUserData();
				if(ecs::scene::IsEntityAlive(_pickedEntity))
				{
					SelectEntity(_pickedEntity);
				}
			}

			JPH::Vec3 normal{ hitBody.GetWorldSpaceSurfaceNormal(res.mSubShapeID2, outPos) };
			renderer->DrawArrow(outPos, outPos + normal, JPH::Color::sDarkBlue, 0.01f);
			JPH::Vec3 tangent{ normal.GetNormalizedPerpendicular() };
			JPH::Vec3 bitangent{ normal.Cross(tangent) };

			renderer->DrawLine(outPos - 0.1f * tangent, outPos + 0.1f * tangent, JPH::Color::sRed);
			renderer->DrawLine(outPos - 0.1f * bitangent, outPos + 0.1f * bitangent, JPH::Color::sYellow);

			JPH::Shape::SupportingFace supportingFace;
			hitBody.GetTransformedShape().GetSupportingFace(res.mSubShapeID2, -normal, JPH::Vec3::sZero(), supportingFace);
			renderer->DrawWirePolygon(JPH::RMat44::sZero(), supportingFace, JPH::Color::sDarkOrange, 0.01f);
		}
	}
	else
	{
		graphics::d3d12::debug::GetDebugRenderer()->DrawMarker((camLT.Position + (camLT.Forward * 0.1f)).Vec3(), JPH::Color::sRed, 0.001f);
	}
	return hit;
}

} // anonymous namespace

void 
UpdateObjectPickerProbe()
{
	const bool interacted{ input::WasKeyPressed(input::Keybinds::Editor.PickObject) };
	JPH::RVec3 hitPos{};
	JPH::BodyID body{};
	f32 fraction{};
	f32 probeLength{ 100.f };
	CastProbe(probeLength, fraction, hitPos, body, interacted);

	if (ecs::scene::IsEntityAlive(_pickedEntity))
	{
		const ecs::component::LocalTransform& lt{ ecs::scene::GetComponent<ecs::component::LocalTransform>(_pickedEntity) };
		assert(ecs::scene::HasComponent<ecs::component::Collider>(_pickedEntity));
		JPH::BodyLockRead lock{ physics::core::PhysicsSystem().GetBodyLockInterface(), ecs::scene::GetComponent<ecs::component::Collider>(_pickedEntity).BodyID };
		if (lock.Succeeded())
		{
			const JPH::Shape* const shape{ lock.GetBody().GetShape() };
			//graphics::d3d12::debug::DrawBodyShape(shape, physics::GetCenterOfMassTransform(lt.Position.Vec3(), lt.Rotation, shape), lt.Scale.Vec3());
		}
	}
}

void Initialize()
{
}

ecs::Entity GetPickedEntity() { return _pickedEntity; }

}