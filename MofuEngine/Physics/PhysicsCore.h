#pragma once
#include "JoltCommon.h"

namespace mofu::physics::core {
namespace settings {
// may want to change motion type when for an entity; this only affects how the achor point is chosen in JPH::FixedConstraint and JPH::SliderConstraint
constexpr bool CREATE_STATIC_BODIES_AS_CHANGEABLE_TO_MOVABLE{ true };
}

void Initialize();
void Shutdown();
void Update(f32 deltaTime);
void FinalizePhysicsWorld();

JPH::BodyInterface& BodyInterface();
JPH::PhysicsSystem& PhysicsSystem();
}