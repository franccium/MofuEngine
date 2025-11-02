#include "PhysicsImporter.h"
#include "Utilities/Logger.h"

namespace mofu::content::physics {

JPH::Ref<JPH::Shape>
CreateJoltMeshFromVertices(Vec<v3>& vertices, const GeometryImportSettings& importSettings)
{
	if (!importSettings.IsStatic || !importSettings.ColliderFromGeometry)
	{
		return nullptr;
	}

	const u32 vertexCount{ (u32)vertices.size() };
	JPH_ASSERT(vertexCount % 3 == 0);
	JPH::TriangleList triangles{};
	triangles.resize(vertexCount / 3);

	for (u32 i{ 0 }, triangleIdx{ 0 }; i < vertexCount; i += 3, triangleIdx++)
	{
		const v3& v1{ vertices[i] };
		const v3& v2{ vertices[i + 1] };
		const v3& v3{ vertices[i + 2] };

		triangles[triangleIdx] = { v1, v2, v3 };
	}

	const JPH::PhysicsMaterial* material{ new JPH::PhysicsMaterial{} };
	JPH::MeshShapeSettings settings{ triangles };
	settings.mMaterials = { material };
	settings.mActiveEdgeCosThresholdAngle = 0.999f;

	JPH::Shape::ShapeResult shapeRes{ settings.Create() };
	if (shapeRes.HasError())
	{
		log::Error("JOLT: Couldn't create mesh shape");
		return nullptr;
	}

	return shapeRes.Get();
}

}
