#include "Light.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/Component.h"

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
	if (coneCos >= math::SQRT12) // cos(pi/4)
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

void
UpdateCullingInfo(CullingInfo& cullingInfo, const CullableLightParameters& params, LightType::Type type)
{
	cullingInfo.Position = params.Position;
	cullingInfo.Range = params.Range;
	cullingInfo.Direction = params.Direction;
	cullingInfo.CosPenumbra = type == LightType::Spot ? params.CosPenumbra : -1.f; // CosPenumbra == -1.f marks a point light
}

} // anonymous namespace

void
ProcessUpdates(u32 lightSetIdx)
{
}

void 
UpdateDirectionalLight(ecs::component::DirectionalLight& l)
{
	LightSet& lightSet{ lightSets[currentLightSetKey] };

	if (!l.Enabled && l.LightDataIndex < lightSet.FirstDisabledNonCullableIndex)
	{
		lightSet.NonCullableLightOwners[l.LightDataIndex].Entity = ecs::Entity{ id::INVALID_ID };
		u32 newDataIdx{ (u32)lightSet.NonCullableLightOwners.size() };
		lightSet.NonCullableLights.emplace_back();
		lightSet.NonCullableLightOwners.emplace_back(l.Owner, LightType::Directional, newDataIdx);
		l.LightDataIndex = newDataIdx;
	}
	else if (l.Enabled && l.LightDataIndex >= lightSet.FirstDisabledNonCullableIndex)
	{
		u32 newDataIdx{ id::INVALID_ID };
		for (u32 i{ 0 }; i < lightSet.FirstDisabledNonCullableIndex; ++i)
		{
			if (lightSet.NonCullableLightOwners[i].Entity == ecs::Entity{ id::INVALID_ID })
			{
				newDataIdx = i;
			}
		}
		if (newDataIdx == id::INVALID_ID)
		{
			lightSet.NonCullableLights.emplace_back();
			lightSet.NonCullableLightOwners.emplace_back();
			newDataIdx = lightSet.FirstDisabledNonCullableIndex;
			memcpy(&lightSet.NonCullableLights[lightSet.FirstDisabledNonCullableIndex + 1],
				&lightSet.NonCullableLights[lightSet.FirstDisabledNonCullableIndex], 
				lightSet.NonCullableLights.size() - lightSet.FirstDisabledNonCullableIndex);
			memcpy(&lightSet.NonCullableLightOwners[lightSet.FirstDisabledNonCullableIndex + 1],
				&lightSet.NonCullableLightOwners[lightSet.FirstDisabledNonCullableIndex],
				lightSet.NonCullableLightOwners.size() - lightSet.FirstDisabledNonCullableIndex);
			lightSet.FirstDisabledNonCullableIndex++;
		}

		lightSet.NonCullableLightOwners[newDataIdx] = { l.Owner, LightType::Directional, newDataIdx };
		l.LightDataIndex = newDataIdx;
	}

	DirectionalLightParameters& params{ lightSet.NonCullableLights[l.LightDataIndex] };
	params.Color = l.Color;
	params.Intensity = l.Intensity;
	params.Direction = l.Direction;
}



void
UpdateCullableLightTransform(const ecs::component::CullableLight& l, const ecs::component::WorldTransform& wt)
{
	LightSet& lightSet{ lightSets[currentLightSetKey] };
	CullableLightParameters& params{ lightSet.CullableLights[l.LightDataIndex] };
	params.Position = { wt.TRS._41, wt.TRS._42, wt.TRS._43 };
	Sphere& sphere{ lightSet.BoundingSpheres[l.LightDataIndex] };
	CalculateBoundingSphere(sphere, params);
	CullingInfo& cullingInfo{ lightSet.CullingInfos[l.LightDataIndex] };
	UpdateCullingInfo(cullingInfo, params, lightSet.CullableLightOwners[l.LightDataIndex].Type);
}

void 
UpdatePointLight(ecs::component::PointLight& l)
{
	LightSet& lightSet{ lightSets[currentLightSetKey] };

	CullableLightParameters& params{ lightSet.CullableLights[l.LightDataIndex] };
	const ecs::component::WorldTransform& wt{
		ecs::scene::GetComponent<ecs::component::WorldTransform>(lightSet.CullableLightOwners[l.LightDataIndex].Entity) };
	params.Color = l.Color;
	params.Intensity = l.Intensity;
	params.Range = l.Range;
	params.Attenuation = l.Attenuation;
	params.Position = { wt.TRS._41, wt.TRS._42, wt.TRS._43 };
	Sphere& sphere{ lightSet.BoundingSpheres[l.LightDataIndex] };
	CalculateBoundingSphere(sphere, params);
	CullingInfo& cullingInfo{ lightSet.CullingInfos[l.LightDataIndex] };
	UpdateCullingInfo(cullingInfo, params, lightSet.CullableLightOwners[l.LightDataIndex].Type);

	lightSet.DirtyBits[l.LightDataIndex].set();
}

void 
UpdateSpotLight(ecs::component::SpotLight& l)
{
	LightSet& lightSet{ lightSets[currentLightSetKey] };

	CullableLightParameters& params{ lightSet.CullableLights[l.LightDataIndex] };
	const ecs::component::LocalTransform& lt{
		ecs::scene::GetComponent<ecs::component::LocalTransform>(lightSet.CullableLightOwners[l.LightDataIndex].Entity) };
	const ecs::component::WorldTransform& wt{
		ecs::scene::GetComponent<ecs::component::WorldTransform>(lightSet.CullableLightOwners[l.LightDataIndex].Entity) };
	params.Color = l.Color;
	params.Intensity = l.Intensity;
	params.Range = l.Range;
	params.Attenuation = l.Attenuation;

	l.Umbra = math::Clamp(l.Umbra, 0.f, math::PI);
	f32 cosUmbra{ DirectX::XMScalarCos(l.Umbra * 0.5f) };
	l.Penumbra = math::Clamp(l.Penumbra, l.Umbra, math::PI);
	f32 cosPenumbra{ DirectX::XMScalarCos(l.Penumbra * 0.5f) };

	params.CosUmbra = cosUmbra;
	params.CosPenumbra = cosPenumbra;
	//params.Position = lt.Position;
	params.Position = { wt.TRS._41, wt.TRS._42, wt.TRS._43 };
	params.Direction = lt.Forward;
	Sphere& sphere{ lightSet.BoundingSpheres[l.LightDataIndex] };
	CalculateBoundingSphere(sphere, params);
	CullingInfo& cullingInfo{ lightSet.CullingInfos[l.LightDataIndex] };
	UpdateCullingInfo(cullingInfo, params, lightSet.CullableLightOwners[l.LightDataIndex].Type);

	lightSet.DirtyBits[l.LightDataIndex].set();
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
	return lightSets[lightSetIdx].FirstDisabledNonCullableIndex;
}

u32 
GetCullableLightsCount(u32 lightSetIdx)
{
	assert(lightSetIdx < lightSets.size());
	return lightSets[lightSetIdx].FirstDisabledCullableIndex;
}

AmbientLightParameters
GetAmbientLight(u32 lightSetIdx)
{
	return lightSets[lightSetIdx].AmbientLight;
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
		auto& dLight{ ecs::scene::GetComponent<ecs::component::DirectionalLight>(lightEntity) };
		//TODO: enabled/disabled 
		u32 dataIndex{ (u32)set.NonCullableLights.size() };
	    set.NonCullableLights.emplace_back(DirectionalLightParameters{ dLight.Direction, dLight.Intensity, dLight.Color });
		set.NonCullableLightOwners.emplace_back(lightEntity, type, dataIndex);
		set.FirstDisabledNonCullableIndex++;
		dLight.LightDataIndex = dataIndex;
		dLight.Owner = lightEntity;
		break;
	}
	case LightType::Point:
	{
		auto& pLight{ ecs::scene::GetComponent<ecs::component::PointLight>(lightEntity) };
		auto& cLight{ ecs::scene::GetComponent<ecs::component::CullableLight>(lightEntity) };
		auto& lt{ ecs::scene::GetComponent<ecs::component::LocalTransform>(lightEntity) };
		//TODO: enabled/disabled 
		u32 dataIndex{ (u32)set.CullableLights.size() };

		set.CullableLights.emplace_back(CullableLightParameters{ lt.Position, pLight.Intensity,
			{0.f, -1.f, 0.f,}, pLight.Color, pLight.Range, pLight.Attenuation });
		set.CullableLightOwners.emplace_back(lightEntity, type, dataIndex);
		set.FirstDisabledCullableIndex++;

		set.CullingInfos.emplace_back();
		set.BoundingSpheres.emplace_back();
		set.DirtyBits.emplace_back();

		CullableLightParameters& params{ set.CullableLights[dataIndex] };
		Sphere& sphere{ set.BoundingSpheres[dataIndex] };
		CalculateBoundingSphere(sphere, params);

		CullingInfo& cullingInfo{ set.CullingInfos[dataIndex] };
		cullingInfo.Range = params.Range;
		cullingInfo.CosPenumbra = type == LightType::Spot ? params.CosPenumbra : -1.f; // CosPenumbra == -1.f marks a point light

		set.DirtyBits[dataIndex].set();
		pLight.LightDataIndex = dataIndex;
		cLight.LightDataIndex = dataIndex;
		pLight.Owner = lightEntity;
		break;
	}
	case LightType::Spot:
	{
		auto& sLight{ ecs::scene::GetComponent<ecs::component::SpotLight>(lightEntity) };
		auto& cLight{ ecs::scene::GetComponent<ecs::component::CullableLight>(lightEntity) };
		auto& lt{ ecs::scene::GetComponent<ecs::component::LocalTransform>(lightEntity) };
		//TODO: enabled/disabled 
		u32 dataIndex{ (u32)set.CullableLights.size() };

		sLight.Umbra = math::Clamp(sLight.Umbra, 0.f, math::PI);
		f32 cosUmbra{ DirectX::XMScalarCos(sLight.Umbra * 0.5f) };
		sLight.Penumbra = math::Clamp(sLight.Penumbra, sLight.Umbra, math::PI);
		f32 cosPenumbra{ DirectX::XMScalarCos(sLight.Penumbra * 0.5f) };

		set.CullableLights.emplace_back(CullableLightParameters{ lt.Position, sLight.Intensity,
			{0.f, -1.f, 0.f,}, sLight.Color, sLight.Range, sLight.Attenuation, cosUmbra, cosPenumbra });
		set.CullableLightOwners.emplace_back(lightEntity, type, dataIndex);
		set.FirstDisabledCullableIndex++;
		set.CullingInfos.emplace_back();
		set.BoundingSpheres.emplace_back();
		set.DirtyBits.emplace_back();

		CullableLightParameters& params{ set.CullableLights[dataIndex] };
		Sphere& sphere{ set.BoundingSpheres[dataIndex] };
		CalculateBoundingSphere(sphere, params);

		CullingInfo& cullingInfo{ set.CullingInfos[dataIndex] };
		UpdateCullingInfo(cullingInfo, params, type);

		set.DirtyBits[dataIndex].set();
		sLight.LightDataIndex = dataIndex;
		cLight.LightDataIndex = dataIndex;
		sLight.Owner = lightEntity;
		break;
	}
	}
}

void 
AddAmbientLight(u32 lightSetIdx, AmbientLightInitInfo ambientInfo)
{
	u32 textureIndices[3]{};
	graphics::GetDescriptorIndices(&ambientInfo.DiffuseTextureID, 3, &textureIndices[0]);
	LightSet& set{ lightSets[lightSetIdx] };
	set.AmbientLight = { ambientInfo.Intensity, textureIndices[0], textureIndices[1], textureIndices[2] };
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