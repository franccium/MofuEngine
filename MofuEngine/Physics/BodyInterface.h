#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include "ECS/Entity.h"

namespace mofu::physics {
JPH::BodyID AddStaticBodyFromMesh(JPH::Ref<JPH::MeshShape> meshShape, ecs::Entity ownerEntity);
JPH::BodyID AddDynamicBody(JPH::Ref<JPH::Shape> shape, ecs::Entity ownerEntity);
}