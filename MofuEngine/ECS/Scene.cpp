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

namespace mofu::ecs::scene {

namespace
{
bool
MatchCet(const CetMask& querySignature, const CetMask& blockSignature)
{
	return (querySignature & blockSignature) == querySignature;
}

std::unordered_map<CetMask, std::vector<EntityBlock*>> queryToBlockMap;
constexpr u32 TEST_ENTITY_COUNT{ 1 }; //TODO: temporarily cause only one entity with render mesh actually has data
constexpr u32 TEST_BLOCK_COUNT{ 5 };
Vec<EntityBlock*> blocks{};

// NOTE: entity IDs globally unique, in format generation | index, index goes into entityData, generation is compared
Vec<EntityData> entityData{};
std::deque<u32> _freeEntityIDs; // TODO: recycling

constexpr size_t ENTITY_BLOCK_SIZE{ 16 * 1024 }; // 16 KiB per block
constexpr size_t ENTITY_BLOCK_ALIGNMENT{ 64 }; // 64 byte alignment
memory::SlabAllocator<ENTITY_BLOCK_SIZE, ENTITY_BLOCK_ALIGNMENT> entityBlockAllocator;
memory::PoolAllocator<EntityBlock> entityBlockHeaderPool;

void RegisterEntityBlock(const CetMask& signature, EntityBlock* block)
{
	for (auto& [querySignature, matchingBlocks] : queryToBlockMap)
	{
		if ((signature & querySignature) == querySignature)
		{
			matchingBlocks.push_back(block);
		}
	}
	// store the Cet somewhere?
}

void
CreateBlock(const CetLayout& layout)
{
	EntityBlock* block{ entityBlockHeaderPool.Allocate() };
	assert(block);

	memcpy(block->ComponentOffsets, layout.ComponentOffsets, sizeof(layout.ComponentOffsets));
	block->Signature = layout.Signature;
	block->Capacity = layout.Capacity;
	block->CetSize = layout.CetSize;
	block->EntityCount = 0;

	block->ComponentData = (u8*)entityBlockAllocator.Allocate();
	block->Entities = reinterpret_cast<Entity*>(block->ComponentData);

	blocks.emplace_back(std::move(block));
}

//TODO: defer operations on EntityBlocks to the end of the frame
void
AddEntity(EntityBlock* b, Entity entity)
{
	// store in first free index of the arrays
	EntityBlock* block{ b };
	u16 row{ block->EntityCount };
	if (row > block->Capacity)
	{
		log::Info("EntityBlock is full");
		assert(false); //TODO: have to create a new block
		// block = ...
		return;
	}

	block->Entities[row] = entity;
	block->EntityCount++;

	u32 idx{ id::Index(entity) };
	entityData.emplace_back(block, row, id::Generation(entity), entity); // TODO: what to do here

	GetEntityComponent<component::LocalTransform>(entity) = component::LocalTransform{};
	//GetEntityComponent<component::LocalTransform>(entity).Position = { -3.0f, -10.f, 10.f}; //TODO: temporary initial transform
	if(idx == 1) GetEntityComponent<component::LocalTransform>(entity).Position = { 0.0f, -10.f, 10.f}; //TODO: temporary initial transform
	log::Info("Added entity %u to block", id::Index(entity));
}

void
RemoveEntity(EntityBlock* block, Entity entity)
{
	// the last entity in block moved in to fill the gap
	// if its the last entity remove the block

	u16 lastRow{ --block->EntityCount };
	EntityData& data{ GetEntityData(entity) };
	if (data.row != lastRow)
	{
		block->Entities[data.row] = block->Entities[lastRow];

		id::AdvanceGeneration((id_t&)block->Entities[data.row]);
		//TODO: copy components over

		Entity movedEntity{ block->Entities[data.row] };
		entityData[id::Index(movedEntity)].row = data.row;
	}
}

} // anonymous namespace

std::vector<EntityBlock*>
GetBlocksFromCet(const CetMask& querySignature)
{
	auto it = queryToBlockMap.find(querySignature);
	if (it != queryToBlockMap.end())
	{
		return it->second;
	}

	// Build the list if not cached
	std::vector<EntityBlock*> result;
	for (auto block : blocks) {
		if ((block->Signature & querySignature) == querySignature) {
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
	assert(entityIdx < entityData.size());
	return entityData[entityIdx];
}
const Vec<EntityData>& GetAllEntityData()
{
	return entityData;
}

EntityData& 
CreateEntity(CetLayout& layout)
{
	EntityBlock* matchingBlock{ nullptr };
	CetMask& signature{ layout.Signature };
	for (u32 i{ 0 }; i < TEST_BLOCK_COUNT; ++i)
	{
		EntityBlock* block{ blocks[i] };
		if (signature == block->Signature) // match the whole signature not just a part of it
		{
			matchingBlock = block;
			break;
		}
	}
	if (!matchingBlock)
	{
		// if no matching block found, create a new one
		CreateBlock(layout);
		matchingBlock = blocks.back();
	}

	AddEntity(matchingBlock, Entity{ (u32)entityData.size() }); // create a new entity with the next ID
	return entityData.back();
}

CetLayout
GenerateCetLayout(const CetMask cetMask)
{
	CetLayout layout{};
	layout.Signature = cetMask;
	layout.Capacity = MAX_ENTITIES_PER_BLOCK;

	u32 currentOffset{ sizeof(Entity) }; // always one entity
	for (ComponentID componentID = 0; componentID < MAX_COMPONENT_TYPES; ++componentID)
	{
		if (layout.Signature.test(componentID))
		{
			layout.ComponentOffsets[componentID] = currentOffset;
			currentOffset += component::GetComponentSize(componentID) * MAX_ENTITIES_PER_BLOCK;
		}
	}
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
AddComponents(EntityData& data, const CetMask& newSignature)
{
	for (u32 i{ 0 }; i < TEST_BLOCK_COUNT; ++i)
	{
		EntityBlock* block{ blocks[i] };
		if (newSignature == block->Signature) // match the whole signature not just a part of it
		{
			// move entity
			//TODO: IMPLEMENT THIS
			/*RemoveEntity(oldBlock, entity);
			AddEntity(&block, entity);*/
			return;
		}
	}

	// if no matching block found, create a new one
	CreateBlock(GenerateCetLayout(newSignature));
	EntityBlock* newBlock{ blocks.back() };
	AddEntity(newBlock, data.id);
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
	for (u32 j{ 0 }; j < TEST_BLOCK_COUNT; ++j)
	{
		CetLayout layout{};
		layout.Capacity = MAX_ENTITIES_PER_BLOCK;
		if (j == 0)
		{
			layout = GenerateCetLayout(GetCetMask<component::LocalTransform, component::WorldTransform>());
		}
		else if (j == 1)
		{
			//layout = GenerateCetLayout(GetCetMask<component::LocalTransform, component::WorldTransform, component::RenderMesh, component::RenderMaterial>());
			layout = GenerateCetLayout(GetCetMask<component::LocalTransform, component::WorldTransform>());
		}
		else if (j == 2)
		{
			layout = GenerateCetLayout(GetCetMask<component::LocalTransform, component::Parent>());
		}
		else if (j == 3)
		{
			layout = GenerateCetLayout(GetCetMask<component::LocalTransform, component::Parent, component::TestComponent4>());
		}
		else
		{
			layout = GenerateCetLayout(GetCetMask<component::LocalTransform>());
		}

		CreateBlock(layout);
		EntityBlock* block{ blocks.back() };
		
		for (u32 i{ 0 }; i < TEST_ENTITY_COUNT; ++i)
		{
			Entity newId{ (u32)entityData.size()};
			Entity newEntity{ newId };

			AddEntity(block, newEntity);
		}
	}

	Entity testEntity{ 0 };
	//AddComponent<component::RenderMesh>(testEntity);
	//AddComponents<component::WorldTransform, component::RenderMesh, component::RenderMaterial>(testEntity);
}

void
TestQueries()
{
	log::Info("Looking for LocalTransform:");
	CetMask querySignature{ GetCetMask<component::LocalTransform>() };
	for (u32 i{ 0 }; i < TEST_BLOCK_COUNT; ++i)
	{
		EntityBlock* block{ blocks[i] };
		if (MatchCet(querySignature, block->Signature))
		{
			std::vector<EntityBlock*> matchingBlocks{ GetBlocksFromCet(querySignature) };
			for (auto block : matchingBlocks)
			{
				log::Info("Found matching block with signature: %s", block->Signature.to_string().c_str());
			}
		}
	}

	log::Info("Looking for TestComponent3 and TestComponent:");
	CetMask querySignature2{ GetCetMask<component::TestComponent3, component::TestComponent>() };
	for (u32 i{ 0 }; i < TEST_BLOCK_COUNT; ++i)
	{
		EntityBlock* block{ blocks[i] };
		if (MatchCet(querySignature2, block->Signature))
		{
			std::vector<EntityBlock*> matchingBlocks{ GetBlocksFromCet(querySignature2) };
			for (auto block : matchingBlocks)
			{
				log::Info("Found matching block with signature: %s", block->Signature.to_string().c_str());
			}
		}
	}

	log::Info("Looking for TestComponent4 and TestComponent2 and TestComponent");
	CetMask querySignature3{ GetCetMask<component::TestComponent4, component::TestComponent, component::TestComponent2>() };
	for (u32 i{ 0 }; i < TEST_BLOCK_COUNT; ++i)
	{
		EntityBlock* block{ blocks[i] };
		if (MatchCet(querySignature3, block->Signature))
		{
			std::vector<EntityBlock*> matchingBlocks{ GetBlocksFromCet(querySignature3) };
			for (auto block : matchingBlocks)
			{
				log::Info("Found matching block with signature: %s", block->Signature.to_string().c_str());
			}
		}
	}
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
	return id::Generation(entityData[id::Index(id)].id) == id::Generation(id);
}

void
Initialize()
{
	//TODO: bake the EntityBlocks from scene data

	FillTestData();

	//TestQueries();

	SerializeTest();
}

void 
Shutdown()
{
}

}