#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include "PhysicsShapes.h"
#include "ECS/Entity.h"

namespace mofu::physics {
void AddStaticBody(JPH::Ref<JPH::Shape> shape, ecs::Entity ownerEntity);
void AddDynamicBody(JPH::Ref<JPH::Shape> shape, ecs::Entity ownerEntity);
void DestroyPhysicsBody(ecs::Entity entity);
void DestroyPhysicsBodies(const Array<ecs::Entity>& entities);
void DeactivatePhysicsBody(ecs::Entity entity);
void UpdateBodyManagerDeferred();

void ChangePhysicsShape(ecs::Entity ownerEntity, shapes::PrimitiveShapes::Type shapeType);
}