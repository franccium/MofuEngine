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

}