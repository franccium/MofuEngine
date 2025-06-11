#pragma once
#include "ECS/QueryView.h"
#include "ECS/Scene.h"

namespace mofu::ecs::scene {

template<typename... Component>
QueryView<true, Component...> GetRW()
{
	auto blocks = GetBlocksFromCet(GetCetMask<Component...>());
	return QueryView<true, Component...>{ std::move(blocks) };
}

template<typename... Component>
QueryView<false, Component...> GetRO()
{
	auto blocks = GetBlocksFromCet(GetCetMask<Component...>());
	return QueryView<true, Component...>{ std::move(blocks) };
}

template<IsComponent C>
C& GetComponent(Entity id)
{
	return GetEntityComponent<C>(id);
}

template<IsComponent C>
void AddComponent(Entity e)
{
	AddEntityComponent<C>(e);
}

// NOTE: sometimes component list is known at compile time, like when spawning from the gamecode
template<IsComponent... C>
//void SpawnEntity(component::InitInfo* infos) // TODO: or whatever, maybe instead just a list of components but idk if that would work
inline EntityData& SpawnEntity(const C... components)
{
	EntityData& entityData{ scene::CreateEntity<C...>() }; // TODO: create an entity, find it a corresponding block and return the EntityData

	((scene::GetEntityComponent<C>(entityData.id) = components), ...);
	return entityData;
}

// NOTE: not all components are known at compile time, for instance with editor, we can select an entity which should
// create an entity with Parent component initialized with the correct parent Entity
inline void SpawnEntity()
{

}

}