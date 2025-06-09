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

class Scene
{

};

template<typename... Component>
CetMask GetCetMask() 
{
	static const CetMask mask = [] {
		CetMask m;
		(m.set(component::ID<Component>), ...);
		return m;
		}();
	return mask;
}

template<IsComponent C>
CetMask PreviewCetMaskPlusComponent(CetMask signature)
{
	//TODO: AddComponents and AddComponent merged or not 
	signature.set(component::ID<C>);
	return signature;
}

template<IsComponent... C>
CetMask PreviewCetMaskPlusComponents(CetMask signature)
{
	//TODO: AddComponents and AddComponent merged or not 
	(signature.set(component::ID<C>), ...);
	return signature;
}

bool IsEntityAlive(Entity id);
EntityData& GetEntityData(Entity id);
const Vec<EntityData>& GetAllEntityData();

std::vector<EntityBlock*> GetBlocksFromCet(const CetMask& querySignature);

template<IsComponent C>
C&
GetEntityComponent(Entity id)
{
	//TODO: this is definitely wrong
	assert(IsEntityAlive(id));
	EntityData data{ GetEntityData(id) };
	return data.block->GetComponentArray<C>()[data.row];
}


void Initialize();
void Shutdown();

}