#include "BodyManager.h"
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include "PhysicsCore.h"
#include "PhysicsLayers.h"
#include "Utilities/Logger.h"
#include "ECS/Transform.h"
#include "ECS/Scene.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCylinderShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/TriangleShape.h>
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>

namespace mofu::physics {
namespace {
struct DeferredBody
{
    ecs::Entity Entity;
    JPH::Ref<JPH::Shape> Shape;
};
Vec<DeferredBody> _deferredDynamicBodies{};
Vec<DeferredBody> _deferredStaticBodies{};

void 
SpawnDeferredBodies()
{
    for (const DeferredBody& b : _deferredDynamicBodies)
    {
        const ecs::Entity e{ b.Entity };
        const ecs::component::LocalTransform& lt{ ecs::scene::GetEntityComponent<ecs::component::LocalTransform>(e) };
        JPH::BodyCreationSettings bodySettings{ b.Shape.GetPtr(), lt.Position.Vec3(), lt.Rotation,
            JPH::EMotionType::Dynamic, PhysicsLayers::Layer::Movable };

        bodySettings.mUserData = e;

        JPH::Body* body{ core::BodyInterface().CreateBody(bodySettings) };
        const JPH::BodyID bodyID{ body->GetID() };
        core::BodyInterface().AddBody(bodyID, JPH::EActivation::Activate);
        log::Info("JOLT: Added a new physics body: [%u]", body->GetID().GetIndex());
        ecs::component::Collider& collider{ ecs::scene::GetEntityComponent<ecs::component::Collider>(e) };
        collider.BodyID = bodyID;


        JPH::BodyLockWrite lock{ physics::core::PhysicsSystem().GetBodyLockInterface(), bodyID };
        lock.GetBody().GetMotionProperties()->SetGravityFactor(0.1f);
    }

    for (const DeferredBody& b : _deferredStaticBodies)
    {
        const ecs::Entity e{ b.Entity };
        const ecs::component::LocalTransform& lt{ ecs::scene::GetEntityComponent<ecs::component::LocalTransform>(e) };
        JPH::BodyCreationSettings bodySettings{ b.Shape.GetPtr(), lt.Position.Vec3(), lt.Rotation,
            JPH::EMotionType::Static, PhysicsLayers::Layer::Static };
        bodySettings.mAllowDynamicOrKinematic = core::settings::CREATE_STATIC_BODIES_AS_CHANGEABLE_TO_MOVABLE;
        bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
        bodySettings.mMassPropertiesOverride = JPH::MassProperties{ core::settings::DEFAULT_BODY_MASS };

        bodySettings.mUserData = e;

        JPH::Body* body{ core::BodyInterface().CreateBody(bodySettings) };
        const JPH::BodyID bodyID{ body->GetID() };
        core::BodyInterface().AddBody(bodyID, JPH::EActivation::DontActivate);
        log::Info("JOLT: Added a new physics body: [%u]", body->GetID().GetIndex());
        ecs::component::Collider& collider{ ecs::scene::GetEntityComponent<ecs::component::Collider>(e) };
        collider.BodyID = bodyID;
    }

    _deferredDynamicBodies.clear();
    _deferredStaticBodies.clear();
}

}

void
AddStaticBody(JPH::Ref<JPH::Shape> shape, ecs::Entity ownerEntity)
{
    ecs::scene::AddComponents<ecs::component::Collider, ecs::component::StaticObject>(ownerEntity);

    _deferredStaticBodies.emplace_back(DeferredBody{ ownerEntity, shape });
}

void
AddDynamicBody(JPH::Ref<JPH::Shape> shape, ecs::Entity ownerEntity)
{
    ecs::scene::AddComponents<ecs::component::Collider, ecs::component::DynamicObject>(ownerEntity);

    //_deferredPhysicsSpawns[_deferredSpawnsCount++] = ownerEntity;
    _deferredDynamicBodies.emplace_back(DeferredBody{ ownerEntity, shape });
}

void 
DestroyPhysicsBody(ecs::Entity ownerEntity)
{
    ecs::component::Collider collider{ ecs::scene::GetEntityComponent<ecs::component::Collider>(ownerEntity) };
    core::BodyInterface().RemoveBody(collider.BodyID);
    core::BodyInterface().DestroyBody(collider.BodyID);
}

void
DeactivatePhysicsBody(ecs::Entity ownerEntity)
{
    ecs::component::Collider collider{ ecs::scene::GetEntityComponent<ecs::component::Collider>(ownerEntity) };
    core::BodyInterface().DeactivateBody(collider.BodyID);
}

void
DestroyPhysicsBodies(const Array<ecs::Entity>& entities)
{
    const u32 entityCount{ (u32)entities.size() };
    Array<JPH::BodyID> bodies{ entityCount };
    for (u32 i{ 0 }; i < entityCount; ++i)
    {
        ecs::component::Collider collider{ ecs::scene::GetEntityComponent<ecs::component::Collider>(entities[i])};
        bodies[i] = collider.BodyID;
    }
    core::BodyInterface().RemoveBodies(bodies.data(), entityCount);
    core::BodyInterface().DestroyBodies(bodies.data(), entityCount);
}

void
UpdateBodyManagerDeferred()
{
    SpawnDeferredBodies();
}

void 
ChangePhysicsShape(ecs::Entity ownerEntity, shapes::PrimitiveShapes::Type shapeType)
{
    ecs::component::Collider& collider{ ecs::scene::GetEntityComponent<ecs::component::Collider>(ownerEntity) };
    //JPH::BodyLockWrite lock{ core::PhysicsSystem().GetBodyLockInterface(), collider.BodyID };
    //if (!lock.Succeeded()) return;
    
    //JPH::Body& body{ lock.GetBody() };

    const ecs::component::LocalTransform& lt{ ecs::scene::GetEntityComponent<ecs::component::LocalTransform>(ownerEntity) };
    JPH::Ref<JPH::Shape> newShape{ nullptr };

    switch (shapeType)
    {
    case shapes::PrimitiveShapes::Box:
    {
        JPH::Vec3 halfExtent(0.5f, 0.5f, 0.5f);
        newShape = new JPH::BoxShape{ halfExtent };
        break;
    }

    case shapes::PrimitiveShapes::Capsule:
    {
        float halfHeight{ 1.0f };
        float radius{ 0.5f };
        newShape = new JPH::CapsuleShape{ halfHeight, radius };
        break;
    }

    case shapes::PrimitiveShapes::Sphere:
    {
        float radius{ 0.5f };
        newShape = new JPH::SphereShape{ radius };
        break;
    }

    case shapes::PrimitiveShapes::Cylinder:
    {
        float halfHeight{ 1.0f };
        float radius{ 0.5f };
        newShape = new JPH::CylinderShape{ halfHeight, radius };
        break;
    }

    case shapes::PrimitiveShapes::Plane:
    {
        JPH::Vec3 normal{ 0.f, 1.f, 0.f };
        f32 constant{ 0.f };
        JPH::Shape::ShapeResult result{};
        newShape = new JPH::PlaneShape{ JPH::Plane{normal, constant}, result };
        assert(result.IsValid());
        break;
    }

    case shapes::PrimitiveShapes::Triangle:
    {
        JPH::Vec3 vertices[]{
            {-0.5f, 0.f, 0.f},
            {0.0f, 0.f, -1.f},
            {0.5f, 0.f, 0.f}
        };
        JPH::TriangleShapeSettings settings{ vertices[0], vertices[1], vertices[2] };
        JPH::Shape::ShapeResult result{};
        newShape = new JPH::TriangleShape{ settings, result };
        assert(result.IsValid());
        break;
    }

    case shapes::PrimitiveShapes::TaperedCapsule:
    {
        float halfHeight{ 1.0f };
        float topRadius{ 0.3f };
        float bottomRadius{ 0.5f };
        JPH::Shape::ShapeResult result{};
        JPH::TaperedCapsuleShapeSettings settings{ halfHeight, topRadius, bottomRadius };
        newShape = new JPH::TaperedCapsuleShape{ settings, result };
        assert(result.IsValid());
        break;
    }

    case shapes::PrimitiveShapes::TaperedCylinder:
    {
        float halfHeight{ 1.0f };
        float topRadius{ 0.3f };
        float bottomRadius{ 0.5f };
        JPH::Shape::ShapeResult result{};
        JPH::TaperedCylinderShapeSettings settings{ halfHeight, topRadius, bottomRadius };
        newShape = new JPH::TaperedCylinderShape{ settings, result };
        assert(result.IsValid());
        break;
    }

    case shapes::PrimitiveShapes::Compound:
    {
        assert(false);
        break;
    }

    case shapes::PrimitiveShapes::ConvexHull:
    {
        JPH::Array<JPH::Vec3> points;
        points.push_back(JPH::Vec3{ -0.5f, -0.5f, -0.5f });
        points.push_back(JPH::Vec3{ 0.5f, -0.5f, -0.5f });
        points.push_back(JPH::Vec3{ -0.5f, 0.5f, -0.5f });
        points.push_back(JPH::Vec3{ 0.5f, 0.5f, -0.5f });
        points.push_back(JPH::Vec3{ -0.5f, -0.5f, 0.5f });
        points.push_back(JPH::Vec3{ 0.5f, -0.5f, 0.5f });
        points.push_back(JPH::Vec3{ -0.5f, 0.5f, 0.5f });
        points.push_back(JPH::Vec3{ 0.5f, 0.5f, 0.5f });

        newShape = JPH::ConvexHullShapeSettings(points).Create().Get();
        break;
    }

    default:
        return;
    }

    if (newShape)
    {
        //const JPH::BodyID bodyID{ body.GetID() };
        core::BodyInterface().SetShape(collider.BodyID, newShape, false, JPH::EActivation::Activate);

        //collider.BodyID = bodyID;
    }
}

}