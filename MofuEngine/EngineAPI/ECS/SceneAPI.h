#pragma once
#include "ECS/QueryView.h"
#include "ECS/Scene.h"
#include "Graphics/Renderer.h"
#include "Utilities/Logger.h"

namespace mofu::ecs::scene {

template<IsComponent C, IsComponent... Cs>
constexpr bool contains_type{ (std::is_same_v<C, Cs> || ...) };

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

	constexpr bool hasRenderMesh{ contains_type<component::RenderMesh, C...> };
	constexpr bool hasRenderMaterial{ contains_type<component::RenderMaterial, C...> };
	if constexpr (hasRenderMesh && hasRenderMaterial)
	{
		component::RenderMesh renderMesh{ scene::GetEntityComponent<component::RenderMesh>(entityData.id) };
		component::RenderMaterial renderMaterial{ scene::GetEntityComponent<component::RenderMaterial>(entityData.id) };
		graphics::AddRenderItem(entityData.id, renderMesh.MeshID, renderMaterial.MaterialCount, renderMaterial.MaterialIDs);
		log::Info("Spawned an entity with Mesh %u and %u Materials", renderMesh.MeshID, renderMaterial.MaterialCount);
	}

	return entityData;
}

// NOTE: not all components are known at compile time, for instance with editor, we can select an entity which should
// create an entity with Parent component initialized with the correct parent Entity
inline void SpawnEntity()
{

}

void DestroyEntity(Entity entity);

void DestroyScene();

struct PrefabEntity
{
	PrefabEntity* Parent{ nullptr };
	PrefabEntity** Children{ nullptr };
	// components
};

}