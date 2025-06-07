#pragma once
#include "ECSCommon.h"
#include <bitset>
#include "Transform.h"
#include "Component.h"

namespace mofu::ecs {
namespace system {
struct SystemUpdateData;
}

constexpr u32 MAX_COMPONENT_TYPES{ 256 };
using CetMask = std::bitset<MAX_COMPONENT_TYPES>;



struct EntityBlock
{
	CetMask signature;
	u32 EntityCount{ 0 };
	void* ComponentBuffers[MAX_COMPONENT_TYPES]{ nullptr }; // TODO: is this too much

	Vec<Entity> entities;
	Vec<id::generation_t> generations;
	component::TransformBlock LocalTransforms; // TODO: have this or not
	std::vector<component::WorldTransform> WorldTransforms; // TODO: have this or not
	//TODO: memory pool for components up to 16 KiB

	template<typename T>
	T* GetComponentArray()
	{
		ComponentBuffers[component::ComponentIDGenerator::GetID<component::LocalTransform>()] = LocalTransforms.LocalTransforms.data(); // FIXME: testing
		ComponentBuffers[component::ComponentIDGenerator::GetID<component::WorldTransform>()] = WorldTransforms.data(); // FIXME: testing
		auto it = ComponentBuffers[component::ComponentIDGenerator::GetID<T>()];
		return reinterpret_cast<T*>(it);
	}

	//template<typename T>	
	//const T* GetComponentArray()
	//{
	//	auto it = ComponentBuffers[component::ComponentIDGenerator::GetID<T>()];
	//	return reinterpret_cast<const T*>(it);
	//}
};

struct EntityData
{
	EntityBlock* block{ nullptr };
	u32 row{ 0 }; // row in the block
	u32 generation{ 0 };
	entity_id id{ 0 };
};

void Initialize();
void Shutdown();

void Update(system::SystemUpdateData data);
}