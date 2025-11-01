#pragma once
#include "CommonHeaders.h"
#include "CommonDefines.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace mofu::physics {
[[nodiscard]] _ALWAYS_INLINE JPH::RMat44
GetCenterOfMassTransform(JPH::RVec3Arg pos, JPH::QuatArg rot, const JPH::Shape* shape, 
	JPH::Vec3Arg offset = JPH::Vec3::sZero(), f32 padding = 0.02f)
{
	return JPH::RMat44::sRotationTranslation(rot, pos).PreTranslated(offset + shape->GetCenterOfMass()).PostTranslated((v3up * padding).Vec3());
}
}