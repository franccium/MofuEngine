#pragma once
#include "Graphics/D3D12/D3D12CommonHeaders.h"
#include "Graphics/Lights/LightsCommon.h"
#include "Graphics/Lights/Light.h"

namespace mofu::graphics::d3d12 {

}

namespace mofu::graphics::d3d12::light {
using DirectionalLightParameters = graphics::d3d12::hlsl::DirectionalLightParameters;
using CullableLightParameters = graphics::d3d12::hlsl::CullableLightParameters;
using CullingInfo = graphics::d3d12::hlsl::LightCullingLightInfo;
using Sphere = graphics::d3d12::hlsl::Sphere;

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

struct D3D12LightSet
{
	u32 NonCullableLightCount{ 0 };
	u32 CullableLightCount{ 0 };
	//Vec<DirectionalLightParameters> NonCullableLights;
	//
	//Vec<CullableLightParameters> CullableLights;
	//Vec<CullingInfo> CullingInfos;
	////TODO: Vec<Sphere>
};

void UpdateLightBuffers(const graphics::light::LightSet& set, u32 lightSetIdx, u32 frameIndex);

u32 GetDirectionalLightsCount(u32 frameIndex);

D3D12_GPU_VIRTUAL_ADDRESS GetNonCullableLightBuffer(u32 frameIndex);
D3D12_GPU_VIRTUAL_ADDRESS GetCullableLightBuffer(u32 frameIndex);
D3D12_GPU_VIRTUAL_ADDRESS GetCullingInfoBuffer(u32 frameIndex);
D3D12_GPU_VIRTUAL_ADDRESS GetBoundingSpheresBuffer(u32 frameIndex);

}