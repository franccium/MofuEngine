#pragma once
#include "CommonHeaders.h"
#include "Entity.h"
#include "Transform.h"

namespace mofu::ecs::transform {

struct EntityFinalTRS
{
	u32 ParentIdx;
	Entity Entity;
//	m4x4 TRS;
	component::WorldTransform* WorldTransform;
	component::LocalTransform* LocalTransform;
};

void AddEntityToHierarchy(Entity entity);
void MoveEntityInHierarchy(Entity entity);
void RemoveEntityFromHierarchy(Entity entity);
void UpdateEntityComponents(Entity entity);

void ReconfigureHierarchy();
void UpdateHierarchy(); // NOTE: called at the end of the frame, after all local transforms have been updated 

// when unloading a scene
void DeleteHierarchy();

const m4x4* const GetPreviousTransform(Entity entity);
}