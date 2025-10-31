#include "BodyInterface.h"
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include "PhysicsCore.h"
#include "PhysicsLayers.h"
#include "Utilities/Logger.h"

JPH::BodyID mofu::physics::AddPhysicsBody(JPH::Ref<JPH::MeshShape> meshShape)
{
    JPH::BodyCreationSettings bodySettings{ meshShape.GetPtr(), JPH::RVec3(0.f, 0.f, 0.f), JPH::Quat::sIdentity(), JPH::EMotionType::Static, PhysicsLayers::Layer::Static};
    JPH::Body* body{ core::BodyInterface().CreateBody(bodySettings) };
    core::BodyInterface().AddBody(body->GetID(), JPH::EActivation::DontActivate);
    log::Info("JOLT: Added new physics body: [%u]", body->GetID().GetIndex());
    return body->GetID();
}
