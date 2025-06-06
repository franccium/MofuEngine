#pragma once
#include "ECSCommon.h"
#include "Transform.h"
#include <bitset>

/*
* has an EntityManager
*/

namespace mofu::ecs {
constexpr u32 MAX_COMPONENT_TYPES{ 256 };
using CetMask = std::bitset<MAX_COMPONENT_TYPES>;

struct EntityBlock
{
	CetMask signature;

	Vec<Entity> entities;
	Vec<id::generation_t> generations;
	component::TransformBlock LocalTransforms;
	//TODO: memory pool for components up to 16 KiB
};

bool IsEntityAlive(entity_id id);

void InitializeECS();
void ShutdownECS();

}