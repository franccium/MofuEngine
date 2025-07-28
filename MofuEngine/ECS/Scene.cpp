#pragma once
#include "Scene.h"
#include "ECSCommon.h"
#include "Transform.h"
#include <bitset>
#include "Utilities/Logger.h"
#include "ECSCore.h"
#include "QueryView.h"
#include "SceneSerializer.h"
#include "Utilities/PoolAllocator.h"
#include "Utilities/SlabAllocator.h"
#include "ComponentRegistry.h"
#include "TransformHierarchy.h"

namespace mofu::ecs::scene {

namespace
{
bool
MatchCet(const CetMask& querySignature, const CetMask& blockSignature)
{
	return (querySignature & blockSignature) == querySignature;
}

Vec<Scene> scenes;
u32 currentSceneIndex;

std::unordered_map<CetMask, Vec<EntityBlock*>> queryToBlockMap;
//constexpr u32 TEST_ENTITY_COUNT{ 1 }; //TODO: temporarily cause only one entity with render mesh actually has data
//constexpr u32 TEST_BLOCK_COUNT{ 5 };
Vec<EntityBlock*> blocks{};

// NOTE: entity IDs globally unique, in format generation | index, index goes into entityData, generation is compared
Vec<EntityData> entityDatas{};
std::deque<u32> _freeEntityIDs; // TODO: recycling

constexpr size_t ENTITY_BLOCK_SIZE{ 32 * 1024 }; // 64 KiB per block
constexpr size_t ENTITY_BLOCK_ALIGNMENT{ 64 }; // 64 byte alignment
memory::SlabAllocator<ENTITY_BLOCK_SIZE, ENTITY_BLOCK_ALIGNMENT> entityBlockAllocator;
memory::PoolAllocator<EntityBlock> entityBlockHeaderPool;

EntityBlock*
CreateBlock(const CetLayout& layout)
{
	EntityBlock* block{ entityBlockHeaderPool.Allocate() };
	assert(block);

	memcpy(block->ComponentOffsets, layout.ComponentOffsets, sizeof(layout.ComponentOffsets));
	block->Signature = layout.Signature;
	block->Capacity = layout.Capacity;
	block->CetSize = layout.CetSize;
	block->EntityCount = 0;

	Vec<ComponentID> componentIDs{};
	for (ComponentID cid = 0; cid < component::ComponentTypeCount; ++cid)
	{
		if (!block->Signature.test(cid)) continue;
		componentIDs.emplace_back(cid);
	}
	u16 componentCount{ (u16)componentIDs.size() };
	block->ComponentCount = componentCount;
	block->ComponentIDs = new ComponentID[componentCount];
	std::copy(componentIDs.begin(), componentIDs.end(), block->ComponentIDs);
	block->ComponentData = (u8*)entityBlockAllocator.Allocate();
	block->Entities = reinterpret_cast<Entity*>(block->ComponentData);

	blocks.emplace_back(std::move(block));
	return blocks.back();
}

//TODO: defer operations on EntityBlocks to the end of the frame
void
AddEntity(Vec<EntityBlock*> matchingBlocks, Entity entity)
{
	// store in first free index of the arrays
	EntityBlock* chosenBlock{ nullptr };
	for (auto b : matchingBlocks)
	{
		u16 row{ b->EntityCount };
		if (row < b->Capacity)
		{
			chosenBlock = b;
			break;
		}
	}
	if(!chosenBlock)
	{
		assert(!matchingBlocks.empty());
		log::Info("EntityBlock is full");
		chosenBlock = CreateBlock(GenerateCetLayout(matchingBlocks[0]->Signature));
	}
	assert(chosenBlock);

	u16 row{ chosenBlock->EntityCount };
	chosenBlock->Entities[row] = entity;
	chosenBlock->EntityCount++;

	u32 idx{ id::Index(entity) };
	entityDatas.emplace_back(chosenBlock, row, id::Generation(entity), entity); // TODO: what to do here
}


void
RemoveEntity(EntityBlock* block, Entity entity)
{
	// the last entity in block moved in to fill the gap
	// if its the last entity remove the block

	const u32 lastRow{ --block->EntityCount };
	if (lastRow == 0)
	{
		// delete the block
		//TODO: anything more?
		blocks.erase_unordered(std::find(blocks.begin(), blocks.end(), block));
		return;
	}

	const EntityData& data{ GetEntityData(entity) };
	if (data.row != lastRow)
	{
		const u32 newRow{ data.row };
		block->Entities[newRow] = block->Entities[lastRow];

		//TODO: i dont yet validate generation
		//id::AdvanceGeneration((id_t&)block->Entities[newRow]);

		// copy over component values
		for (ComponentID cid : block->GetComponentView())
		{
			const u32 componentSize{ component::GetComponentSize(cid) };
			const u32 oldOffset{ block->ComponentOffsets[cid] + componentSize * lastRow };
			const u32 newOffset{ block->ComponentOffsets[cid] + componentSize * newRow };
			memcpy(block->ComponentData + newOffset, block->ComponentData + oldOffset, componentSize);
		}

		Entity movedEntity{ block->Entities[newRow] };
		entityDatas[id::Index(movedEntity)].row = newRow;
	}
}


void
MigrateEntity(EntityData& entityData, EntityBlock* oldBlock, EntityBlock* newBlock)
{
	const Entity entity{ entityData.id };
	const u32 oldRow{ entityData.row };
	const u32 newRow{ newBlock->EntityCount };

	// copy over component values
	for (ComponentID cid : oldBlock->GetComponentView())
	{
		if (newBlock->Signature.test(cid))
		{
			const u32 componentSize{ component::GetComponentSize(cid) };
			const u32 oldOffset{ oldBlock->ComponentOffsets[cid] + componentSize * oldRow };
			const u32 newOffset{ newBlock->ComponentOffsets[cid] + componentSize * newRow };
			memcpy(newBlock->ComponentData + newOffset, oldBlock->ComponentData + oldOffset, component::GetComponentSize(cid));
		}
	}

	RemoveEntity(oldBlock, entity);
	entityData.block = newBlock;
	entityData.row = newRow;
	newBlock->Entities[newRow] = entity;
	newBlock->EntityCount++;

	//TODO: to avoid updating all possible references, implement the generations finally
	if (EntityHasComponent<component::WorldTransform>(entity))
	{
		UpdateEntityTransformComponents(entity);
	}
}

} // anonymous namespace

Vec<EntityBlock*>
GetBlocksFromCet(const CetMask& querySignature)
{
	//TODO: caching has to be updated, also just better query mechanism overall is needed
	//auto it = queryToBlockMap.find(querySignature);
	//if (it != queryToBlockMap.end())
	//{
	//	return it->second;
	//}

	// Build the list if not cached
	Vec<EntityBlock*> result;
	for (auto block : blocks)
	{
		if ((block->Signature & querySignature) == querySignature)
		{
			result.push_back(block);
		}
	}
	queryToBlockMap[querySignature] = result;
	return queryToBlockMap[querySignature];
}

EntityData&
GetEntityData(Entity id)
{
	assert(IsEntityAlive(id));
	u32 entityIdx{ id::Index(id) };
	assert(entityIdx < entityDatas.size());
	return entityDatas[entityIdx];
}
const Vec<EntityData>& GetAllEntityData()
{
	return entityDatas;
}

EntityData&
CreateEntity(const CetMask& signature)
{
	Vec<EntityBlock*> matchingBlocks{};
	for (EntityBlock* b : blocks)
	{
		if (signature == b->Signature) // match the whole signature not just a part of it
		{
			matchingBlocks.emplace_back(b);
		}
	}
	if (matchingBlocks.empty())
	{
		// if no matching block found, create a new one
		CetLayout layout{ GenerateCetLayout(signature) };

		EntityBlock* b{ CreateBlock(layout) };
		matchingBlocks.emplace_back(b);
	}

	AddEntity(matchingBlocks, Entity{ (u32)entityDatas.size() }); // create a new entity with the next ID
	return entityDatas.back();
}

const Scene& 
GetCurrentScene()
{
	return scenes[currentSceneIndex];
}

void 
CreateScene()
{
	scenes.emplace_back(Scene{ (u32)scenes.size() });
}

CetLayout
GenerateCetLayout(const CetMask& cetMask)
{
	CetLayout layout{};
	layout.Signature = cetMask;
	layout.Capacity = MAX_ENTITIES_PER_BLOCK;

	u32 currentOffset{ sizeof(Entity) * MAX_ENTITIES_PER_BLOCK }; // always one entity
	for (ComponentID componentID = 0; componentID < component::ComponentTypeCount; ++componentID)
	{
		if (layout.Signature.test(componentID))
		{
			layout.ComponentOffsets[componentID] = currentOffset;
			currentOffset += component::GetComponentSize(componentID) * MAX_ENTITIES_PER_BLOCK;
		}
	}
	assert(currentOffset < ENTITY_BLOCK_SIZE);
	layout.CetSize = currentOffset / MAX_ENTITIES_PER_BLOCK;

	return layout;
}

//template<IsComponent... C>
//void
//AddComponents(Entity entity)
//{
//	assert(IsEntityAlive(entity));
//	EntityData& data{ GetEntityData(entity) };
//	EntityBlock* oldBlock{ data.block };
//
//	CetMask newSignature{ PreviewCetMaskPlusComponents<C...>(oldBlock->Signature) };
//	for (u32 i{ 0 }; i < TEST_BLOCK_COUNT; ++i)
//	{
//		EntityBlock& block{ blocks[i] };
//		if (newSignature == block.Signature) // match the whole signature not just a part of it
//		{
//			// move entity
//			//TODO: IMPLEMENT THIS
//			/*RemoveEntity(oldBlock, entity);
//			AddEntity(&block, entity);*/
//			return;
//		}
//	}
//
//	// if no matching block found, create a new one
//	blocks.emplace_back(std::move(*CreateBlock(GenerateCetLayout(newSignature))));
//	EntityBlock* newBlock{ &blocks.back() };
//	AddEntity(newBlock, entity);
//}

void
AddComponents(EntityData& data, const CetMask& newSignature, EntityBlock* oldBlock)
{
	for (EntityBlock* b : blocks)
	{
		if (newSignature == b->Signature && b->EntityCount < b->Capacity) // match the whole signature not just a part of it
		{
			// move entity
			//TODO: IMPLEMENT THIS
			MigrateEntity(data, oldBlock, b);
			return;
		}
	}

	// if no matching block found, create a new one
	EntityBlock* newBlock{ CreateBlock(GenerateCetLayout(newSignature)) };
	Vec<EntityBlock*> b{};
	b.emplace_back(newBlock);
	MigrateEntity(data, oldBlock, newBlock);
}

//template<IsComponent C>
//void
//AddComponent(Entity entity)
//{
//	//NOTE: get blocks from cet caches queries that match parts of the signature so can't use that here
//	//TODO: AddComponents and AddComponent merged or not 
//	
//	// if a matching Cet after adding the component doesn't exist, create it
//	// else move the entity to the corresponding EntityBlock
//	//assert(IsEntityAlive(entity));
//	//EntityData& data{ GetEntityData(entity) };
//	//EntityBlock* oldBlock{ data.block };
//
//	//CetMask newSignature{ PreviewCetMaskPlusComponent<C>(oldBlock->signature) };
//	//for (u32 i{ 0 }; i < TEST_BLOCK_COUNT; ++i)
//	//{
//	//	EntityBlock& block{ blocks[i] };
//	//	if (newSignature == block.signature) // match the whole signature not just a part of it
//	//	{
//	//		// move entity
//	//		block.entities.emplace_back(entity);
//	//		block.generations.emplace_back(id::Generation(entity));
//	//		block.EntityCount++;
//	//		block.LocalTransforms.LocalTransforms.emplace_back(GetEntityComponent<component::LocalTransform>(entity)); //TODO: what about transform
//	//		return;
//	//	}
//	//}
//
//	//// if no matching block found, create a new one
//	//blocks.emplace_back(EntityBlock{ newSignature });
//	//EntityBlock& newBlock{ blocks.back() };
//	//newBlock.LocalTransforms.LocalTransforms.emplace_back(GetEntityComponent<component::LocalTransform>(entity)); //TODO: what about transform
//	//newBlock.EntityCount = 1;
//	//newBlock.generations.emplace_back(id::Generation(entity));
//	//newBlock.entities.emplace_back(entity);
//	//data.block = &newBlock;
//	//data.generation = 0;
//	//data.row = 0;
//}

void
FillTestData()
{
	// create 5 entities with LocalTransforms
	//constexpr u32 TEST_BLOCK_COUNT{ 5 };
	//for (u32 j{ 0 }; j < TEST_BLOCK_COUNT; ++j)
	//{
	//	CetLayout layout{};
	//	layout.Capacity = MAX_ENTITIES_PER_BLOCK;
	//	if (j == 0)
	//	{
	//		layout = GenerateCetLayout(GetCetMask<component::LocalTransform, component::WorldTransform>());
	//	}
	//	else if (j == 1)
	//	{
	//		//layout = GenerateCetLayout(GetCetMask<component::LocalTransform, component::WorldTransform, component::RenderMesh, component::RenderMaterial>());
	//		layout = GenerateCetLayout(GetCetMask<component::LocalTransform, component::WorldTransform>());
	//	}
	//	else if (j == 2)
	//	{
	//		layout = GenerateCetLayout(GetCetMask<component::LocalTransform, component::Parent>());
	//	}
	//	else if (j == 3)
	//	{
	//		layout = GenerateCetLayout(GetCetMask<component::LocalTransform, component::Parent, component::TestComponent4>());
	//	}
	//	else
	//	{
	//		layout = GenerateCetLayout(GetCetMask<component::LocalTransform>());
	//	}

	//	EntityBlock* block{ CreateBlock(layout) };
	//	
	//	for (u32 i{ 0 }; i < 1; ++i)
	//	{
	//		Entity newId{ (u32)entityData.size()};
	//		Entity newEntity{ newId };

	//		AddEntity(block, newEntity);
	//	}
	//}

	//Entity testEntity{ 0 };
	//AddComponent<component::RenderMesh>(testEntity);
	//AddComponents<component::WorldTransform, component::RenderMesh, component::RenderMaterial>(testEntity);
}

void 
SerializeTest()
{
	
}

bool
IsEntityAlive(Entity id)
{
	assert(id::IsValid(id));
	// if the generation doesn't match, the entity had to die/never exist
	return id::Generation(entityDatas[id::Index(id)].id) == id::Generation(id);
}

void 
RemoveEntity(Entity entity)
{
	assert(IsEntityAlive(entity));
	u32 entityIdx{ id::Index(entity) };
	//TODO:
}

void 
UnloadScene()
{

}

void
Initialize()
{
	CreateScene();

	//TODO: bake the EntityBlocks from scene data

	//FillTestData();

	//TestQueries();

	//SerializeTest();
}

void 
Shutdown()
{
}

}