#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>

namespace mofu::physics {
JPH::BodyID AddPhysicsBody(JPH::Ref<JPH::MeshShape> meshShape);
}