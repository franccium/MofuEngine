#pragma once
#include "ECSCommon.h"
#include <bitset>
#include "Transform.h"
#include "Component.h"
//#include "Graphics/D3D12/D3D12Core.h"
#include "ComponentRegistry.h"
#include <span>

namespace mofu::graphics::d3d12 {
struct D3D12FrameInfo;
}

namespace mofu::ecs {
namespace system {
struct SystemUpdateData;
}

constexpr u32 MAX_ENTITIES_PER_BLOCK{ 128 };

struct CetLayout
{
	CetMask Signature;
	u32 CetSize{ 0 };
	u16 Capacity{ 0 };
	u32 ComponentOffsets[MAX_COMPONENT_TYPES]{ sizeof(Entity) * MAX_ENTITIES_PER_BLOCK }; // there is always one entity, so the first offset is sizeof(Entity)
};

struct EntityBlock
{
	CetMask Signature;
	u16 EntityCount{ 0 };
	u16 Capacity{ 0 }; // up to 128, maybe less based on component size
	u32 CetSize{ 0 };
	u16 ComponentCount{};
	ComponentID* ComponentIDs{};
	Entity* Entities; // the first array in ComponentData
	//id::generation_t* Generations;
	u8* ComponentData{ nullptr };
	u32 ComponentOffsets[MAX_COMPONENT_TYPES]{ sizeof(Entity) * MAX_ENTITIES_PER_BLOCK }; // there is always one entity, so the first offset is sizeof(Entity)

	template<IsComponent C>
	C* GetComponentArray()
	{
		u32 offset = ComponentOffsets[component::ID<C>];
		assert(offset);
		return reinterpret_cast<C*>(ComponentData + offset);
	}

	inline std::span<ComponentID> GetComponentView() const { return { ComponentIDs, ComponentCount }; }

	~EntityBlock()
	{
		if (ComponentIDs)
		{
			delete[] ComponentIDs;
			ComponentIDs = nullptr;
		}
	}
};

struct EntityData
{
	EntityBlock* block{ nullptr };
	u32 row{ 0 }; // row in the block
	u32 generation{ 0 };
	Entity id{ id::INVALID_ID };
};

template<typename Fun>
void ForEachComponent(const EntityBlock* const block, u32 row, Fun&& func)
{
	for (ComponentID cid : block->GetComponentView())
	{
		u32 offset = block->ComponentOffsets[cid] + component::GetComponentSize(cid) * row;
		u8* componentData = block->ComponentData + offset;

		std::invoke(std::forward<Fun>(func), cid, componentData);
	}
}

void Initialize();
void Shutdown();

void UpdatePrePhysics(system::SystemUpdateData data);
void UpdatePostPhysics(system::SystemUpdateData data);
//TODO: temporary solution
void UpdateRenderSystems(system::SystemUpdateData data, const graphics::d3d12::D3D12FrameInfo& d3d12FrameInfo);
}