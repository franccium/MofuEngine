#include "ResourceCreation.h"
#include <array>

namespace mofu::content {
namespace {

id_t
CreateMeshResource(const void* const blob)
{
	return id::INVALID_ID;
}

void
DestroyMeshResource(id_t id)
{

}

#pragma region not_implemented
id_t 
CreateUnknown([[maybe_unused]] const void* const blob)
{
	assert(false);
	return id::INVALID_ID;
}

id_t
CreateTextureResource([[maybe_unused]] const void* const blob)
{
	assert(false);
	return id::INVALID_ID;
}

id_t
CreateAnimationResource([[maybe_unused]] const void* const blob)
{
	assert(false);
	return id::INVALID_ID;
}

id_t
CreateAudioResource([[maybe_unused]] const void* const blob)
{
	assert(false);
	return id::INVALID_ID;
}

id_t
CreateMaterialResource([[maybe_unused]] const void* const blob)
{
	assert(false);
	return id::INVALID_ID;
}

id_t
CreateSkeletonResource([[maybe_unused]] const void* const blob)
{
	assert(false);
	return id::INVALID_ID;
}

void
DestroyUnknown([[maybe_unused]] id_t id)
{
	assert(false);
}

void
DestroyTextureResource([[maybe_unused]] id_t id)
{
	assert(false);
}

void
DestroyAnimationResource([[maybe_unused]] id_t id)
{
	assert(false);
}

void
DestroyAudioResource([[maybe_unused]] id_t id)
{
	assert(false);
}

void
DestroyMaterialResource([[maybe_unused]] id_t id)
{
	assert(false);
}

void
DestroySkeletonResource([[maybe_unused]] id_t id)
{
	assert(false);
}
#pragma endregion

using ResourceCreator = id_t(*)(const void* const blob);
constexpr std::array<ResourceCreator, AssetType::Count> resourceCreators{
	CreateUnknown,
	CreateMeshResource,
	CreateTextureResource,
	CreateAnimationResource,
	CreateAudioResource,
	CreateMaterialResource,
	CreateSkeletonResource,
};
static_assert(resourceCreators.size() == AssetType::Count, "Resource creator array size mismatch");
using ResourceDestructor = void(*)(id_t id);
constexpr std::array<ResourceDestructor, AssetType::Count> resourceDestructors{
	DestroyUnknown,
	DestroyMeshResource,
	DestroyTextureResource,
	DestroyAnimationResource,
	DestroyAudioResource,
	DestroyMaterialResource,
	DestroySkeletonResource,
};
static_assert(resourceDestructors.size() == AssetType::Count, "Resource destructor array size mismatch");

} // anonymous namespace

id_t
CreateResourceFromBlob(const char* const blob, AssetType::type resourceType)
{
	assert(blob);
	return resourceCreators[resourceType](blob);
}

void
DestroyResource(id_t resourceId, AssetType::type resourceType)
{
	assert(id::IsValid(resourceId));
	resourceDestructors[resourceType](resourceId);
}
}