#pragma once
#include "ECSCommon.h"
#include "Transform.h"
#include "ECSCore.h"
#include "Component.h"
#include "Utilities/Logger.h"
/*
* has an EntityManager
*/

namespace mofu::ecs::scene {

template<typename... Component>
CetMask GetCetMask() 
{
	static const CetMask mask = [] {
		CetMask m;
		(m.set(component::ComponentIDGenerator::GetID<Component>()), ...);
		return m;
		}();
	return mask;
}

bool IsEntityAlive(entity_id id);
EntityData GetEntityData(entity_id id);

std::vector<EntityBlock*> GetBlocksFromCet(const CetMask& querySignature);

template<typename C>
concept IsComponent = std::derived_from<C, component::Component>;

template<IsComponent C>
C&
GetEntityComponent(entity_id id)
{
	//TODO: this is definitely wrong
	assert(IsEntityAlive(id));
	EntityData data{ GetEntityData(id) };
	auto arr = data.block->GetComponentArray<C>();
	return data.block->GetComponentArray<C>()[data.row];
}


void Initialize();
void Shutdown();

}