#pragma once
#include "../Renderer.h"
#include "../GraphicsTypes.h"

#include "ECS/Entity.h"

namespace mofu::graphics::light {
using DirectionalLightParameters = graphics::d3d12::hlsl::DirectionalLightParameters;
using CullableLightParameters = graphics::d3d12::hlsl::CullableLightParameters;
using CullingInfo = graphics::d3d12::hlsl::LightCullingLightInfo;
using Sphere = graphics::d3d12::hlsl::Sphere;

struct LightType
{
	enum Type : u32
	{
		Directional,
		Point,
		Spot,

		Count
	};
};

struct LightOwner
{
	ecs::Entity Entity{ id::INVALID_ID };
	LightType::Type Type;
	u32 DataIndex;
};

struct LightSet
{
	u32 FirstDisabledNonCullableIndex{ 0 };
	Vec<LightOwner> NonCullableLightOwners;
	Vec<DirectionalLightParameters> NonCullableLights; // just an array
	u32 FirstDirtyCullableIndex{ 0 };
	u32 FirstDisabledCullableIndex{ 0 };

	Vec<LightOwner> CullableLightOwners; // sorted, clean - dirty - disabled
	Vec<CullableLightParameters> CullableLights; // sorted, clean - dirty - disabled
	Vec<CullingInfo> CullingInfos; // sorted, clean - dirty - disabled
	Vec<Sphere> BoundingSpheres; // sorted, clean - dirty - disabled
};

bool Initialize();
void Shutdown();

void ProcessUpdates(u32 lightSetIdx);
void SendLightData();

const LightSet& GetLightSet(u32 lightSetIdx);
u32 GetCurrentLightSetKey();

u32 CreateLightSet();
void RemoveLightSet(u32 lightSetIdx);
void AddLightToLightSet(u32 lightSetIdx, ecs::Entity lightEntity, LightType::Type type);

u32 GetDirectionalLightsCount(u32 lightSetIdx);
}