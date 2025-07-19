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
	return QueryView<true, Component...>(blocks);
}

template<typename... Component>
QueryView<false, Component...> GetRO()
{
	auto blocks = GetBlocksFromCet(GetCetMask<Component...>());
	return QueryView<true, Component...>(blocks);
}

template<IsComponent C>
C& GetComponent(Entity id)
{
	return GetEntityComponent<C>(id);
}

template<IsComponent C>
bool HasComponent(Entity id)
{
	return EntityHasComponent<C>(id);
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

	//constexpr bool hasRenderMesh{ contains_type<component::RenderMesh, C...> };
	//constexpr bool hasRenderMaterial{ contains_type<component::RenderMaterial, C...> };
	//if constexpr (hasRenderMesh && hasRenderMaterial)
	//{
	//	component::RenderMesh renderMesh{ scene::GetEntityComponent<component::RenderMesh>(entityData.id) };
	//	component::RenderMaterial renderMaterial{ scene::GetEntityComponent<component::RenderMaterial>(entityData.id) };
	//	graphics::AddRenderItem(entityData.id, renderMesh.MeshID, renderMaterial.MaterialCount, renderMaterial.MaterialIDs);
	//}

	return entityData;
}

inline EntityData& SpawnEntity(const CetMask& signature)
{
	EntityData& entityData{ scene::CreateEntity(signature) };

	return entityData;
}

void DestroyEntity(Entity entity);

void DestroyScene();

u32 GetRenderItemCount();
void GetRenderItemIDs(Vec<id_t>& outIDs);

}