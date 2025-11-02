#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Core/Reference.h>
#include <filesystem>
#include "Content/Asset.h"

namespace mofu::physics::shapes {
struct PrimitiveShapes
{
	enum Type : u8
	{
		Box,
		Capsule,
		Sphere,
		Cylinder,
		Plane,
		Triangle,
		TaperedCapsule,
		TaperedCylinder,
		Compound,
		ConvexHull,
		Count
	};
};

void LoadShape(const u8* const blob);
[[nodiscard]] JPH::Ref<JPH::Shape> LoadShape(content::AssetHandle handle);
[[nodiscard]] JPH::Ref<JPH::Shape> LoadShape(const std::filesystem::path& path);
[[nodiscard]] content::AssetHandle SaveShape(const JPH::Shape* shape, const std::filesystem::path& path);
JPH::Ref<JPH::Shape> GetShape();
}