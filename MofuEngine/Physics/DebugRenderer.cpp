#include "DebugRenderer.h"
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

namespace mofu::graphics::d3d12::debug {
namespace {

} // anonymous namespace

DebugRenderer::DebugRenderer()
{
}

void
DebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
{
}

void 
DebugRenderer::DrawTriangle(JPH::RVec3Arg v1, JPH::RVec3Arg v2, JPH::RVec3Arg v3, JPH::ColorArg color, ECastShadow castShadow)
{
}

void 
DebugRenderer::DrawText3D(JPH::RVec3Arg position, const std::string_view& string, JPH::ColorArg color, f32 height)
{
}

void 
DebugRenderer::DrawGeometry(JPH::RMat44Arg modelMatrix, const JPH::AABox& worldSpaceBounds, f32 LODScaleSq, JPH::ColorArg modelColor, const GeometryRef& geometry, ECullMode cullMode, ECastShadow castShadow, EDrawMode drawMode)
{
}

DebugRenderer::Batch
DebugRenderer::CreateTriangleBatch(const Triangle* triangles, s32 triangleCount)
{
	return Batch();
}

DebugRenderer::Batch
DebugRenderer::CreateTriangleBatch(const Vertex* vertices, s32 vertexCount, const u32* indices, s32 indexCount)
{
	return Batch();
}

void 
Render()
{

}

}