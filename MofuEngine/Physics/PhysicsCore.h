#pragma once
#include "CommonHeaders.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace mofu::physics::core {
void Initialize();
void Shutdown();
void Update(f32 deltaTime);
void FinalizePhysicsWorld();

JPH::BodyInterface& BodyInterface();
}