#pragma once
#include "CommonHeaders.h"

/*
* Updates to transform will first change the values stored in TransformCache, and the changes will be applied 
* at the end of the frame, when writes to TransformCache are blocked; only the flagged elements will be updated
* this helps with multithreading and lets the different modules (physics engine, various scripts) to read and modify the data
* TODO: module transform update priority
*/

namespace mofu::ecs::component {
// for roots this would be world space, for children local
struct InitInfo
{

};

// used for rendering
struct WorldTransform
{

};

// exposed to various systems as read-only or read-write
struct LocalTransform
{
	math::v3 position{ 0.0f, 0.0f, 0.0f };
	math::v3 rotation{ 0.0f, 0.0f, 0.0f };
	math::v3 scale{ 1.0f, 1.0f, 1.0f };
	math::v3 forward{ 0.0f, 0.0f, 1.0f };
};

struct TestComponent
{
	math::v4 data{};
	u32 data2{};
};

struct TransformBlock
{
	//TODO: sotre in a way that lets only use dirty transforms or like optimize for unmoveable entities 
	Vec<LocalTransform> localTransforms;

	void ReserveSpace(u32 entityCount)
	{
		localTransforms.reserve(entityCount);
	}
};
}