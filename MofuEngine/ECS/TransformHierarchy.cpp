#include "TransformHierarchy.h"
#include "Scene.h"
#include "SystemMessages.h"

namespace mofu::ecs::transform {
namespace {

/*
* a table for each corresponding number of parents influencing the final transform
* the tables are ordered based on the index of influencing entity in the i-1 table
* Vec[0] = transforms of entities with no parents
* Vec[1] = with one parent
* ...
*/
//NOTE: for now, im assuming that every entity needs a WorldTransform, which is not true
Vec<Vec<EntityFinalTRS>> finalTransforms{};

// this could be more packed cause it's only needed for motion vectors so for entities that have a WorldTransform; 
// unless i find some nice other usage for previous transforms
Vec<m4x4> _previousTransforms{};

//TODO: when i stack all added entities to add them at the end i should just have one sort instead of inserting in the right place immediately
constexpr bool SORT_DEFERRED{ false };

u32
GetEntityIndex(Entity entity, u32 level)
{
	const Vec<EntityFinalTRS>& transforms{ finalTransforms[level] };
	for (u32 i = 0; i < transforms.size(); ++i)
	{
		if (transforms[i].Entity == entity) return i;
	}
	assert(false);
	return 0;
}

EntityFinalTRS&
FindEntityFinalTRS(Entity entity)
{
	Entity currentEntity{ entity };
	u32 parentCount{ 0 };
	while (ecs::scene::EntityHasComponent<ecs::component::Child>(currentEntity))
	{
		currentEntity = ecs::scene::GetEntityComponent<ecs::component::Child>(currentEntity).ParentEntity;
		parentCount++;
	}

	return finalTransforms[parentCount][GetEntityIndex(entity, parentCount)];
}

} // anonymous namespace

void 
AddEntityToHierarchy(Entity entity)
{
	Entity currentEntity{ entity };
	u32 parentCount{ 0 };
	while (ecs::scene::EntityHasComponent<ecs::component::Child>(currentEntity))
	{
		currentEntity = ecs::scene::GetEntityComponent<ecs::component::Child>(currentEntity).ParentEntity;
		parentCount++;
	}

	EntityFinalTRS finalTRS{};
	finalTRS.Entity = entity;
	finalTRS.ParentIdx = parentCount ? GetEntityIndex(ecs::scene::GetEntityComponent<ecs::component::Child>(entity).ParentEntity, parentCount - 1) : 0;
	assert(ecs::scene::EntityHasComponent<ecs::component::WorldTransform>(entity));
	assert(ecs::scene::EntityHasComponent<ecs::component::LocalTransform>(entity));
	finalTRS.WorldTransform = &ecs::scene::GetEntityComponent<ecs::component::WorldTransform>(entity);
	finalTRS.LocalTransform = &ecs::scene::GetEntityComponent<ecs::component::LocalTransform>(entity);

	_previousTransforms.emplace_back();
	assert(id::Index(entity) < _previousTransforms.size());
	_previousTransforms[id::Index(entity)] = {};

	if constexpr (SORT_DEFERRED)
	{

	}
	else
	{
		if (finalTransforms.size() <= parentCount) finalTransforms.emplace_back();

		Vec<EntityFinalTRS>& transforms{ finalTransforms[parentCount] };
		for (u32 i = 0; i < transforms.size(); ++i)
		{
			if (finalTRS.ParentIdx < transforms[i].ParentIdx)
			{
				finalTransforms[parentCount].insert(i, finalTRS);
				return;
			}
		}

		transforms.emplace_back(finalTRS);
	}
}

void
MoveEntityInHierarchy(Entity entity)
{
}

void
RemoveEntityFromHierarchy(Entity entity)
{
}

void 
UpdateEntityComponents(Entity entity)
{
	//TODO: the Entity values might change depending on how i implement generations
	EntityFinalTRS& entityTransform{ FindEntityFinalTRS(entity) };
	assert(ecs::scene::EntityHasComponent<ecs::component::WorldTransform>(entity));
	assert(ecs::scene::EntityHasComponent<ecs::component::LocalTransform>(entity));
	entityTransform.WorldTransform = &ecs::scene::GetEntityComponent<ecs::component::WorldTransform>(entity);
	entityTransform.LocalTransform = &ecs::scene::GetEntityComponent<ecs::component::LocalTransform>(entity);
}

void
ReconfigureHierarchy()
{
}

void 
UpdateHierarchy()
{
	using namespace DirectX;
	if (finalTransforms.empty()) return;
	bool transformChanged{ false };

	for (auto& rootTransform : finalTransforms[0])
	{
		component::LocalTransform* lt{ rootTransform.LocalTransform };
		component::WorldTransform* wt{ rootTransform.WorldTransform };

		xmm scale{ XMLoadFloat3(&lt->Scale) };
		xmm rot{ XMLoadFloat4(&lt->Rotation) };
		xmm pos{ XMLoadFloat3(&lt->Position) };
		xmm dir{ 0.f, 0.f, 1.f, 0.f };
		XMStoreFloat3(&lt->Forward, XMVector3Normalize(XMVector3Rotate(dir, rot))); //TODO: this belong to local transform updates

		memcpy(&_previousTransforms[id::Index(rootTransform.Entity)], &wt->TRS, sizeof(m4x4));
		xmmat trs{XMMatrixAffineTransformation(scale, g_XMZero, rot, pos) };
		XMStoreFloat4x4(&wt->TRS, trs);

		if (_previousTransforms[id::Index(rootTransform.Entity)]._41 != wt->TRS._41)
		{
			log::Info("Change");
		}
	}

	for (u32 level{ 1 }; level < finalTransforms.size(); ++level)
	{
		auto& transforms{ finalTransforms[level] };
		auto& parents{ finalTransforms[level - 1] };
		for (auto& finalTRS : transforms)
		{
			assert(finalTRS.ParentIdx < parents.size());
			component::WorldTransform* parentWt{ parents[finalTRS.ParentIdx].WorldTransform};

			component::LocalTransform* lt{ finalTRS.LocalTransform };
			component::WorldTransform* wt{ finalTRS.WorldTransform };
			
			xmm scale{ XMLoadFloat3(&lt->Scale) };
			xmm rot{ XMLoadFloat4(&lt->Rotation) };
			xmm pos{ XMLoadFloat3(&lt->Position) };
			xmm dir{ 0.f, 0.f, 1.f, 0.f };
			XMStoreFloat3(&lt->Forward, XMVector3Normalize(XMVector3Rotate(dir, rot))); //TODO: this belong to local transform updates

			memcpy(&_previousTransforms[id::Index(finalTRS.Entity)], &wt->TRS, sizeof(m4x4));
			xmmat trs{XMMatrixAffineTransformation(scale, g_XMZero, rot, pos) };
			xmmat parentTrs{ XMLoadFloat4x4(&parentWt->TRS) };
			trs = XMMatrixMultiply(parentTrs, trs);
			XMStoreFloat4x4(&wt->TRS, trs);
		}
	}

	messages::SetMessage(messages::SystemBoolMessage::TransformChanged, transformChanged);
}

void 
DeleteHierarchy()
{
	finalTransforms.clear();
}

const m4x4* const
GetPreviousTransform(Entity entity)
{
	assert(id::Index(entity) < _previousTransforms.size());
	return &_previousTransforms[id::Index(entity)];
}


}
