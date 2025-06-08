#pragma once
#include "CommonHeaders.h"
#include "Component.h"

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
struct WorldTransform : Component
{
	m4x4 TRS{};
};

struct RenderMesh : Component
{
	m4x4 TRS{};
};

struct RenderMaterial : Component
{
	m4x4 TRS{};
};

// exposed to various systems as read-only or read-write
struct LocalTransform : Component
{
	v3 Position{ 0.0f, 0.0f, 0.0f };
	v3 Rotation{ 0.0f, 0.0f, 0.0f };
	v3 Scale{ 1.0f, 1.0f, 1.0f };
	v3 Forward{ 0.0f, 0.0f, 1.0f };
};

static f32 count; // TODO: testing
struct TransformBlock
{
	//TODO: sotre in a way that lets only use dirty transforms or like optimize for unmoveable entities 
	// or just versions instead?
	Vec<LocalTransform> LocalTransforms;

	void ReserveSpace(u32 entityCount)
	{
		LocalTransforms.resize(entityCount);
		for (auto& transform : LocalTransforms)
		{
			transform.Position = { -3.f, -10.f, 10.f };
			//transform.Position.x = count;

			count += 0.02f;
		}
	}
};
}