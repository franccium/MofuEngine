#pragma once
#include "CommonHeaders.h"
#include "../Renderer.h"

#include "../GraphicsTypes.h"
#include "ECS/Entity.h"

namespace mofu::graphics::light {
using DirectionalLightParameters = graphics::d3d12::hlsl::DirectionalLightParameters;
using CullableLightParameters = graphics::d3d12::hlsl::CullableLightParameters;

struct LightTypes
{
	enum Type : u32
	{
		Directional,
		Point,
		Spot,

		Count
	};
};

class LightSet
{
	Vec<DirectionalLightParameters> _nonCullableLights;
	Vec<CullableLightParameters> _cullableLights;
};

bool Initialize();
void Shutdown();

LightSet& GetLightSet(u64 lightSetKey);

void CreateLightSet(u64 lightSetKey);
void RemoveLightSet(u64 lightSetKey);
void AddLightToLightSet(ecs::Entity lightEntity);

}