#pragma once
#include "CommonHeaders.h"
#include "Entity.h"
#include "Transform.h"

namespace mofu::ecs {

struct EntityFinalTRS
{
	u32 ParentIdx;
	Entity Entity;
//	m4x4 TRS;
	component::WorldTransform* WorldTransform;
	component::LocalTransform* LocalTransform;
};

void AddEntityToTransformHierarchy(Entity entity);
void MoveEntityInTransformHierarchy(Entity entity);
void RemoveEntityFromTransformHierarchy(Entity entity);
void UpdateEntityTransformComponents(Entity entity);

void ReconfigureTransformHierarchy();
void UpdateTransformHierarchy(); // NOTE: called at the end of the frame, after all local transforms have been updated 
}