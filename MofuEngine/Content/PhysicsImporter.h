#pragma once
#include "CommonHeaders.h"
#include "Graphics/GeometryData.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>

namespace mofu::content::physics {
JPH::Ref<JPH::MeshShape> CreateJoltMeshFromVertices(Vec<v3>& vertices, const GeometryImportSettings& importSettings);
}