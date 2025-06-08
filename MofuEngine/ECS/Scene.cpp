#pragma once
#include "Scene.h"
#include "ECSCommon.h"
#include "Transform.h"
#include <bitset>
#include "Utilities/Logger.h"
#include "ECSCore.h"
#include "QueryView.h"

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
Vec<EntityBlock> blocks(TEST_BLOCK_COUNT);

// NOTE: entity IDs globally unique, in format generation | index, index goes into entityData, generation is compared
Vec<EntityData> entityData{};
std::deque<u32> _freeEntityIDs; // TODO: recycling

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
	for (auto& block : blocks) {
		if ((block.signature & querySignature) == querySignature) {
			result.push_back(&block);
		}
	}
	queryToBlockMap[querySignature] = result;
	return queryToBlockMap[querySignature];
}

EntityData 
GetEntityData(entity_id id)
{
	assert(IsEntityAlive(id));
	u32 entityIdx{ id::Index(id) };
	assert(entityIdx < entityData.size());
	return entityData[entityIdx];
}

void
FillTestData()
{
	// create 5 entities with LocalTransforms
	for (u32 j{ 0 }; j < TEST_BLOCK_COUNT; ++j)
	{
		EntityBlock& block{ blocks[j] };

		if (j == 0)
		{
			block.signature = GetCetMask<component::LocalTransform, component::TestComponent, component::WorldTransform>();
		}
		else if (j == 1)
		{
			block.signature = GetCetMask<component::LocalTransform, component::TestComponent,
				component::WorldTransform, component::RenderMesh, component::RenderMaterial>();
			//block.signature = GenerateCetMask<component::LocalTransform, component::TestComponent, 
			//	component::WorldTransform>();
			//block.signature = GenerateCetMask<component::LocalTransform, component::WorldTransform, 
			//	component::TestComponent>();
		}
		else if (j == 2)
		{
			block.signature = GetCetMask<component::LocalTransform, component::TestComponent2, component::TestComponent4>();
		}
		else if (j == 3)
		{
			block.signature = GetCetMask<component::LocalTransform, component::TestComponent, component::TestComponent3, component::TestComponent2>();
		}
		else
		{
			block.signature = GetCetMask<component::LocalTransform>();
		}

		block.LocalTransforms.ReserveSpace(TEST_ENTITY_COUNT);

		for (u32 i{ 0 }; i < TEST_ENTITY_COUNT; ++i)
		{
			entity_id newId{ (u32)entityData.size()};
			Entity newEntity{ newId };
			block.generations.emplace_back(0);

			block.entities.emplace_back(newEntity);
			block.EntityCount++;

			//TODO: do sth with this
			block.WorldTransforms.emplace_back(component::WorldTransform{});
			if (j == 1)
			{
				// add TestComponent to the second EntityBlock
			}

			// local transform can be left default

			//u32 idx{ i + j * TEST_ENTITY_COUNT };
			u32 idx{ id::Index(newEntity.ID)};
			entityData.emplace_back(&block, i, newEntity.ID);
		}
	}
}

void
TestQueries()
{
	log::Info("Looking for LocalTransform:");
	CetMask querySignature{ GetCetMask<component::LocalTransform>() };
	for (u32 i{ 0 }; i < TEST_BLOCK_COUNT; ++i)
	{
		EntityBlock& block{ blocks[i] };
		if (MatchCet(querySignature, block.signature))
		{
			std::vector<EntityBlock*> matchingBlocks{ GetBlocksFromCet(querySignature) };
			for (auto& block : matchingBlocks)
			{
				log::Info("Found matching block with signature: %s", block->signature.to_string().c_str());
			}
		}
	}

	log::Info("Looking for TestComponent3 and TestComponent:");
	CetMask querySignature2{ GetCetMask<component::TestComponent3, component::TestComponent>() };
	for (u32 i{ 0 }; i < TEST_BLOCK_COUNT; ++i)
	{
		EntityBlock& block{ blocks[i] };
		if (MatchCet(querySignature2, block.signature))
		{
			std::vector<EntityBlock*> matchingBlocks{ GetBlocksFromCet(querySignature2) };
			for (auto& block : matchingBlocks)
			{
				log::Info("Found matching block with signature: %s", block->signature.to_string().c_str());
			}
		}
	}

	log::Info("Looking for TestComponent4 and TestComponent2 and TestComponent");
	CetMask querySignature3{ GetCetMask<component::TestComponent4, component::TestComponent, component::TestComponent2>() };
	for (u32 i{ 0 }; i < TEST_BLOCK_COUNT; ++i)
	{
		EntityBlock& block{ blocks[i] };
		if (MatchCet(querySignature3, block.signature))
		{
			std::vector<EntityBlock*> matchingBlocks{ GetBlocksFromCet(querySignature3) };
			for (auto& block : matchingBlocks)
			{
				log::Info("Found matching block with signature: %s", block->signature.to_string().c_str());
			}
		}
	}
}

bool
IsEntityAlive(entity_id id)
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
}

void 
Shutdown()
{
}

}