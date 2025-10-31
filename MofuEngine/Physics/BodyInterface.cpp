#include "BodyInterface.h"
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include "PhysicsCore.h"
#include "PhysicsLayers.h"
#include "Utilities/Logger.h"
#include "ECS/Transform.h"
#include "ECS/Scene.h"

namespace mofu::physics {
JPH::BodyID 
AddStaticBodyFromMesh(JPH::Ref<JPH::MeshShape> meshShape, ecs::Entity ownerEntity)
{
    ecs::scene::AddComponents<ecs::component::Collider, ecs::component::StaticObject>(ownerEntity);

    const ecs::component::LocalTransform& lt{ ecs::scene::GetEntityComponent<ecs::component::LocalTransform>(ownerEntity) };
    JPH::BodyCreationSettings bodySettings{ meshShape.GetPtr(), lt.Position.AsJPVec3(), lt.Rotation, 
        JPH::EMotionType::Static, PhysicsLayers::Layer::Static};
    bodySettings.mUserData = ownerEntity;

    JPH::Body* body{ core::BodyInterface().CreateBody(bodySettings) };
    const JPH::BodyID bodyID{ body->GetID() };
    core::BodyInterface().AddBody(bodyID, JPH::EActivation::DontActivate);
    log::Info("JOLT: Added a new physics body: [%u]", body->GetID().GetIndex());
    ecs::component::Collider& collider{ ecs::scene::GetEntityComponent<ecs::component::Collider>(ownerEntity) };
    collider.BodyID = bodyID;

    return bodyID;
}

JPH::BodyID
AddDynamicBody(JPH::Ref<JPH::Shape> meshShape, ecs::Entity ownerEntity)
{
    ecs::scene::AddComponents<ecs::component::Collider, ecs::component::DynamicObject>(ownerEntity);

    const ecs::component::LocalTransform& lt{ ecs::scene::GetEntityComponent<ecs::component::LocalTransform>(ownerEntity) };
    JPH::BodyCreationSettings bodySettings{ meshShape.GetPtr(), lt.Position.AsJPVec3(), lt.Rotation,
        JPH::EMotionType::Dynamic, PhysicsLayers::Layer::Movable };
    bodySettings.mUserData = ownerEntity;

    JPH::Body* body{ core::BodyInterface().CreateBody(bodySettings) };
    const JPH::BodyID bodyID{ body->GetID() };
    core::BodyInterface().AddBody(bodyID, JPH::EActivation::Activate);
    log::Info("JOLT: Added a new physics body: [%u]", body->GetID().GetIndex());
    ecs::component::Collider& collider{ ecs::scene::GetEntityComponent<ecs::component::Collider>(ownerEntity) };
    collider.BodyID = bodyID;

    return bodyID;
}
}