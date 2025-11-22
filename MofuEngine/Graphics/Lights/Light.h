#pragma once
#include "../Renderer.h"
#include "../GraphicsTypes.h"

#include "ECS/Entity.h"
//#include "ECS/Transform.h"
#include <bitset>

namespace mofu::ecs::component {
	struct WorldTransform;
	struct DirectionalLight;
	struct CullableLight;
	struct PointLight;
	struct SpotLight;
}

namespace mofu::graphics::light {
using DirectionalLightParameters = graphics::d3d12::hlsl::DirectionalLightParameters;
using CullableLightParameters = graphics::d3d12::hlsl::CullableLightParameters;
using CullingInfo = graphics::d3d12::hlsl::LightCullingLightInfo;
using Sphere = graphics::d3d12::hlsl::Sphere;
using AmbientLightParameters = graphics::d3d12::hlsl::AmbientLightParameters;

struct LightType
{
	enum Type : u8
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

constexpr u32 FRAME_BUFFER_COUNT{ 3 }; // FIXME:
struct LightSet
{
	AmbientLightParameters AmbientLight;

	u32 FirstDisabledNonCullableIndex{ 0 };
	Vec<LightOwner> NonCullableLightOwners;
	Vec<DirectionalLightParameters> NonCullableLights; // just an array
	u32 FirstDirtyCullableIndex{ 0 };
	u32 FirstDisabledCullableIndex{ 0 };
	u32 SkyboxSrvIndex{ U32_INVALID_ID };
	id_t EnvironmentMapTextureID{ U32_INVALID_ID };
	id_t BrdfLutTextureID{ U32_INVALID_ID };

	Vec<LightOwner> CullableLightOwners; // sorted, clean - dirty - disabled
	Vec<CullableLightParameters> CullableLights; // sorted, clean - dirty - disabled
	Vec<CullingInfo> CullingInfos; // sorted, clean - dirty - disabled
	Vec<Sphere> BoundingSpheres; // sorted, clean - dirty - disabled
	Vec<std::bitset<FRAME_BUFFER_COUNT>> DirtyBits; // NOTE: don't like having this
};

bool Initialize();
void Shutdown();

void UpdateCullableLightTransform(const ecs::component::CullableLight& l, const ecs::component::WorldTransform& wt);
void UpdateDirectionalLight(ecs::component::DirectionalLight& l);
void UpdatePointLight(ecs::component::PointLight& l);
void UpdateSpotLight(ecs::component::SpotLight& l);
void ProcessUpdates(u32 lightSetIdx);
void SendLightData();

const LightSet& GetLightSet(u32 lightSetIdx);
u32 GetCurrentLightSetKey();
u32* const GetCurrentLightSetKeyRef();
f32* const GetAmbientIntensityRef();

u32 CreateLightSet();
void RemoveLightSet(u32 lightSetIdx);
void AddLightToLightSet(u32 lightSetIdx, ecs::Entity lightEntity, LightType::Type type);

void AddAmbientLight(u32 lightSetIdx, AmbientLightInitInfo ambientInfo);
AmbientLightParameters GetAmbientLight(u32 lightSetIdx);

u32 GetDirectionalLightsCount(u32 lightSetIdx);
u32 GetCullableLightsCount(u32 lightSetIdx);
}