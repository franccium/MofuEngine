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
public:
	explicit Scene(u32 id) : _id{ id } { memset(Name, 0, sizeof(Name)); }

	constexpr u32 GetSceneID() const { return _id; }
#if EDITOR_BUILD
	static constexpr u32 MAX_NAME_LENGTH{ 32 };
	char Name[MAX_NAME_LENGTH];
#endif
private:
	u32 _id;
};

const Scene& GetCurrentScene();
void CreateScene(const char* scenename);

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

CetLayout GenerateCetLayout(const CetMask& cetMask);

bool IsEntityAlive(Entity id);
EntityData& GetEntityData(Entity id);
const Vec<EntityData>& GetAllEntityData();

Vec<EntityBlock*> GetBlocksFromCet(const CetMask& querySignature);

template<IsComponent C>
C&
GetEntityComponent(Entity id)
{
	//TODO: this is definitely wrong
	assert(IsEntityAlive(id));
	EntityData data{ GetEntityData(id) };
	return data.block->GetComponentArray<C>()[data.row];
}

template<IsComponent C>
bool
EntityHasComponent(Entity e)
{
	const EntityData& data{ GetEntityData(e) };
	return data.block->Signature.test(component::ID<C>);
}

void AddComponents(EntityData& data, const CetMask& newSignature, EntityBlock* oldBlock);

template<IsComponent... C>
void
AddComponents(Entity entity)
{
	assert(IsEntityAlive(entity));
	EntityData& data{ GetEntityData(entity) };
	EntityBlock* oldBlock{ data.block };
	CetMask newSignature{ PreviewCetMaskPlusComponents<C...>(oldBlock->Signature) };
	
	AddComponents(data, newSignature, oldBlock);
}

template<IsComponent C>
void
AddEntityComponent(Entity e)
{
	assert(IsEntityAlive(e));
	AddComponents<C>(e);
}

EntityData& CreateEntity(const CetMask& signature);

template<IsComponent... C>
EntityData& CreateEntity()
{
	const CetMask cetMask{ GetCetMask<C...>() };
	return CreateEntity(cetMask);
}

// returns the total count of entities where Cet includes all specified components
template<IsComponent... C>
u32 GetEntityCount()
{
	const CetMask cetMask{ GetCetMask<C...>() };
	u32 count{ 0 };
	for (EntityBlock* block : GetBlocksFromCet(cetMask))
	{
		count += block->EntityCount;
	}
	return count;
}

void RemoveEntity(Entity entity);
void UnloadScene();

void Initialize();
void Shutdown();

}