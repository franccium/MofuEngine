#include "Light.h"
#include "EngineAPI/ECS/SceneAPI.h"

namespace mofu::graphics::light {
namespace {

u32 currentLightSetKey{};
util::FreeList<LightSet> lightSets{};

void 
CalculateBoundingSphere(Sphere& sphere, const CullableLightParameters& params)
{
	using namespace DirectX;
	xmm tip{ XMLoadFloat3(&params.Position) };
	xmm dir{ XMLoadFloat3(&params.Direction) };
	const f32 coneCos{ params.CosPenumbra };
	if (coneCos >= 0.707107f) // cos(pi/4)
	{
		sphere.Radius = params.Range / (2.f * coneCos);
		XMStoreFloat3(&sphere.Center, tip + sphere.Radius * dir);
	}
	else
	{
		XMStoreFloat3(&sphere.Center, tip + coneCos * params.Range * dir);
		const f32 coneSin{ sqrt(1.f - coneCos * coneCos) };
		sphere.Radius = coneSin * params.Range;
	}
}

} // anonymous namespace

void
ProcessUpdates(u32 lightSetIdx)
{
}

void
SendLightData()
{
}

u32 
CreateLightSet()
{
	return lightSets.add();
}

void 
RemoveLightSet(u32 lightSetIdx)
{
	assert(lightSetIdx < lightSets.size());
	lightSets.remove(lightSetIdx);
}

u32 
GetDirectionalLightsCount(u32 lightSetIdx)
{
	assert(lightSetIdx < lightSets.size());
	LightSet& set{ lightSets[lightSetIdx] };
	return set.FirstDisabledNonCullableIndex;
}

void
AddLightToLightSet(u32 lightSetIdx, ecs::Entity lightEntity, LightType::Type type)
{
	assert(lightSetIdx < lightSets.size());
	LightSet& set{ lightSets[lightSetIdx] };

	//TODO: this will not work
	switch (type)
	{
	case LightType::Directional:
	{
		auto& light{ ecs::scene::GetComponent<ecs::component::Light>(lightEntity) };
		auto& dLight{ ecs::scene::GetComponent<ecs::component::DirectionalLight>(lightEntity) };
		//TODO: enabled/disabled 
		u32 dataIndex{ (u32)set.NonCullableLights.size() };
	    set.NonCullableLights.emplace_back(DirectionalLightParameters{ dLight.Direction, light.Intensity, light.Color });
		set.NonCullableLightOwners.emplace_back(lightEntity, type, dataIndex);
		set.FirstDisabledNonCullableIndex++;
		break;
	}
	case LightType::Point:
	{
		auto& light{ ecs::scene::GetComponent<ecs::component::Light>(lightEntity) };
		auto& pLight{ ecs::scene::GetComponent<ecs::component::PointLight>(lightEntity) };
		auto& lt{ ecs::scene::GetComponent<ecs::component::LocalTransform>(lightEntity) };
		//TODO: enabled/disabled 
		u32 dataIndex{ (u32)set.CullableLights.size() };

		set.CullableLights.emplace_back(CullableLightParameters{ lt.Position, light.Intensity, 
			{0.f, -1.f, 0.f,}, light.Color, pLight.Range, pLight.Attenuation });
		set.CullableLightOwners.emplace_back(lightEntity, type, dataIndex);
		set.FirstDisabledCullableIndex++;

		set.CullingInfos.emplace_back();
		set.BoundingSpheres.emplace_back();

		CullableLightParameters& params{ set.CullableLights[dataIndex] };
		Sphere& sphere{ set.BoundingSpheres[dataIndex] };
		CalculateBoundingSphere(sphere, params);

		CullingInfo& cullingInfo{ set.CullingInfos[dataIndex] };
		cullingInfo.Range = params.Range;
		cullingInfo.CosPenumbra = type == LightType::Spot ? params.CosPenumbra : -1.f;
		break;
	}
	case LightType::Spot:
	{
		auto& light{ ecs::scene::GetComponent<ecs::component::Light>(lightEntity) };
		auto& sLight{ ecs::scene::GetComponent<ecs::component::SpotLight>(lightEntity) };
		auto& lt{ ecs::scene::GetComponent<ecs::component::LocalTransform>(lightEntity) };
		//TODO: enabled/disabled 
		u32 dataIndex{ (u32)set.CullableLights.size() };

		sLight.Umbra = math::Clamp(sLight.Umbra, 0.f, math::PI);
		f32 cosUmbra{ DirectX::XMScalarCos(sLight.Umbra * 0.5f) };
		sLight.Penumbra = math::Clamp(sLight.Penumbra, sLight.Umbra, math::PI);
		f32 cosPenumbra{ DirectX::XMScalarCos(sLight.Penumbra * 0.5f) };

		set.CullableLights.emplace_back(CullableLightParameters{ lt.Position, light.Intensity,
			{0.f, -1.f, 0.f,}, light.Color, sLight.Range, sLight.Attenuation, cosUmbra, cosPenumbra });
		set.CullableLightOwners.emplace_back(lightEntity, type, dataIndex);
		set.FirstDisabledCullableIndex++;
		set.CullingInfos.emplace_back();
		set.BoundingSpheres.emplace_back();

		CullableLightParameters& params{ set.CullableLights[dataIndex] };
		Sphere& sphere{ set.BoundingSpheres[dataIndex] };
		CalculateBoundingSphere(sphere, params);

		CullingInfo& cullingInfo{ set.CullingInfos[dataIndex] };
		cullingInfo.Range = params.Range;
		cullingInfo.CosPenumbra = type == LightType::Spot ? params.CosPenumbra : -1.f;
		break;
	}
	}
}

const LightSet&
GetLightSet(u32 lightSetIdx)
{
	assert(lightSetIdx < lightSets.size());
	return lightSets[lightSetIdx];
}

u32 
GetCurrentLightSetKey()
{
	return currentLightSetKey;
}


}