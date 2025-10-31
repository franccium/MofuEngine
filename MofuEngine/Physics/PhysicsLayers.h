#pragma once
#include "CommonHeaders.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace mofu::physics {
	struct PhysicsLayers
	{
		enum Layer : JPH::ObjectLayer
		{
			Static,
			Movable,
			Count
		};
	};

	namespace bphLayers {
		static constexpr JPH::BroadPhaseLayer Static{ 0 };
		static constexpr JPH::BroadPhaseLayer Movable{ 1 };
		static constexpr u32 LayerCount{ 2 };
	}
}