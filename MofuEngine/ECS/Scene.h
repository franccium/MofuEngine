#pragma once
#include "ECSCommon.h"
#include "Transform.h"

/*
* has an EntityManager
*/

namespace mofu::ecs {

struct EntityBlock
{
	Vec<Entity> entities;
	Vec<id::generation_t> generations;
	component::TransformBlock localTransforms;
	//TODO: memory pool for components up to 16 KiB

};


constexpr u32 TEST_ENTITY_COUNT{ 5 };
constexpr u32 TEST_BLOCK_COUNT{ 2 };
Vec<EntityBlock> blocks(TEST_BLOCK_COUNT);
struct EntityData
{
	EntityBlock* block{ nullptr };
	entity_id id{ 0 };
};
Vec<EntityData> entityData{TEST_ENTITY_COUNT * TEST_BLOCK_COUNT};

void 
FillTestData()
{
	// create 5 entities with LocalTransforms
	for (u32 j{ 0 }; j < TEST_BLOCK_COUNT; ++j)
	{
		EntityBlock& block{ blocks[j] };
		block.localTransforms.ReserveSpace(TEST_ENTITY_COUNT);

		for (u32 i{ 0 }; i < TEST_ENTITY_COUNT; ++i)
		{
			entity_id newId{ (id_t)block.generations.size() };
			Entity newEntity{ newId };
			block.generations.emplace_back(0);

			block.entities.emplace_back(newEntity);
			if (j == 1)
			{
				// add TestComponent to the second EntityBlock
			}

			// local transform can be left default

			u32 idx{ i + j * TEST_ENTITY_COUNT };
			entityData[idx].block = &blocks[j];
			entityData[idx].id = newEntity.id;
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
InitializeECS()
{
	//TODO: bake the EntityBlocks from scene data

	FillTestData();
	
}
}