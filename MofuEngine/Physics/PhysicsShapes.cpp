#include "PhysicsShapes.h"
#include "Utilities/IOStream.h"
#include "Utilities/Logger.h"
#include <fstream>
#include <Jolt/Core/StreamWrapper.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include "Content/EditorContentManager.h"

namespace mofu::physics::shapes {
namespace {
util::FreeList<JPH::Ref<JPH::Shape>> _shapeRefs{};

} // anonymous namespace

void
LoadShape(const u8* const blob)
{
	util::BlobStreamReader{ blob };
	assert(false);
}

JPH::Ref<JPH::Shape> LoadShape(content::AssetHandle handle)
{
	auto asset{ content::assets::GetAsset(handle) };
	return LoadShape(asset->ImportedFilePath);
}

JPH::Ref<JPH::Shape>
LoadShape(const std::filesystem::path& path)
{
	//TODO: check IsAssetAlreadyRegistered
	std::ifstream file{path, std::ios::binary };
	auto stream{ JPH::StreamInWrapper{ file } };
	JPH::Shape::IDToShapeMap shapeMap{};
	JPH::Shape::IDToMaterialMap materialMap{};

	JPH::Shape::ShapeResult result{ JPH::Shape::sRestoreWithChildren(stream, shapeMap, materialMap) };
	if (result.HasError())
	{
		log::Error("Could not load physics shape ($s)", path.string());
		return nullptr;
	}

	return result.Get();
}

content::AssetHandle 
SaveShape(const JPH::Shape* shape, const std::filesystem::path& path)
{
	//TODO: check IsAssetAlreadyRegistered
	std::ofstream file{ path, std::ios::binary };
	auto stream{ JPH::StreamOutWrapper{file} };
	JPH::Shape::ShapeToIDMap shapeMap{};
	JPH::Shape::MaterialToIDMap materialMap{};

	shape->SaveWithChildren(stream, shapeMap, materialMap);


	content::Asset* asset = new content::Asset{ content::AssetType::PhysicsShape, path, path };
	log::Info("Created Physics Shape Asset: (%s)", path.string().c_str());
	return content::assets::RegisterAsset(asset);
}

JPH::Ref<JPH::Shape> 
GetShape(id_t shapeID)
{
	assert(_shapeRefs.size() < shapeID && id::IsValid(shapeID));
	return _shapeRefs[shapeID];
}

}
