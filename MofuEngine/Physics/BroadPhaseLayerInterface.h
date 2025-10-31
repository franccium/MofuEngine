#pragma once
#include "PhysicsLayers.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

namespace mofu::physics {
	class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer obj1Layer, JPH::ObjectLayer obj2Layer) const override
		{
			switch (obj1Layer)
			{
			case PhysicsLayers::Static: return obj1Layer == obj2Layer;
			case PhysicsLayers::Movable: return true;
			default:
				JPH_ASSERT(false);
				return false;
			}
		}
	};

	class ObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer objLayer, JPH::BroadPhaseLayer bpLayer) const override
		{
			switch (objLayer)
			{
			case PhysicsLayers::Static: return bpLayer == bphLayers::Movable;
			case PhysicsLayers::Movable: return true;
			default:
				JPH_ASSERT(false);
				return false;
			}
		}
	};

	class BroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
	{
	public:
		BroadPhaseLayerInterface()
		{
			_objectToBroadPhase[PhysicsLayers::Static] = bphLayers::Static;
			_objectToBroadPhase[PhysicsLayers::Movable] = bphLayers::Movable;
		}

		[[nodiscard]] u32 GetNumBroadPhaseLayers() const override { return bphLayers::LayerCount; }

		[[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(const JPH::ObjectLayer objLayer) const override
		{
			JPH_ASSERT(objLayer < PhysicsLayers::Count);
			return _objectToBroadPhase[objLayer];
		}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		const char* GetBroadPhaseLayerName(const JPH::BroadPhaseLayer bphLayer) const override 
		{
			switch (static_cast<JPH::BroadPhaseLayer::Type>(bphLayer)) {
			case static_cast<JPH::BroadPhaseLayer::Type>(bphLayers::Static):
				return "Static";
			case static_cast<JPH::BroadPhaseLayer::Type>(bphLayers::Movable):
				return "Movable";
			default: JPH_ASSERT(false);
				return "Undefined";
			}
		}
#endif

	private:
		JPH::BroadPhaseLayer _objectToBroadPhase[PhysicsLayers::Count];
	};
}