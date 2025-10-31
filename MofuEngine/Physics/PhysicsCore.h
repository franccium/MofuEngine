#pragma once
#include "JoltCommon.h"

namespace mofu::physics::core {
void Initialize();
void Shutdown();
void Update(f32 deltaTime);
void FinalizePhysicsWorld();

JPH::BodyInterface& BodyInterface();
JPH::PhysicsSystem& PhysicsSystem();
}