#pragma once
#include "Physics/PhysicsShapes.h"
#include "ECS/Entity.h"

namespace mofu::editor::physics::shapes {
void DisplayMotionTypeOptions(ecs::Entity entity);
void DisplayColliderOptions(ecs::Entity entity);
void DisplayColliderEditor(ecs::Entity entity);
}