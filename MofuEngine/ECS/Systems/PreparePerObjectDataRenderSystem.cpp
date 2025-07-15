#pragma once
#include "ECS/ECSCommon.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"

#include "Graphics/GraphicsTypes.h"
#include "Graphics/D3D12/D3D12Content.h"
#include "Graphics/D3D12/D3D12Resources.h"
#include "Graphics/D3D12/D3D12GPass.h"
#include "Graphics/D3D12/D3D12Content/D3D12Geometry.h"
#include "Graphics/D3D12/D3D12Content/D3D12Material.h"
#include "Graphics/D3D12/D3D12Content/D3D12Texture.h"

#include "ECS/Transform.h"
#include "Utilities/Logger.h"

namespace mofu::graphics::d3d12 {
struct PreparePerObjectDataSystem : ecs::system::System<PreparePerObjectDataSystem>
{
	//TODO: figure out caching stuff and not updating unchanged
	void Update([[maybe_unused]] const ecs::system::SystemUpdateData data)
	{
		//log::Info("PreparePerObjectDataSystem::Update");

		//ConstantBuffer& cbuffer{ core::CBuffer() };

		//for (auto [entity, transform, renderable] : ecs::scene::GetRW<ecs::component::WorldTransform, ecs::component::Renderable>())
		//{
		//	using namespace DirectX;

		//	hlsl::PerObjectData* data{ cbuffer.AllocateSpace<hlsl::PerObjectData>() };

		//	xmmat transformWorld{ XMLoadFloat4x4(&transform.TRS) };
		//	XMStoreFloat4x4(&data->World, transformWorld);
		//	transformWorld.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);
		//	xmmat inverseWorld{ XMMatrixInverse(nullptr, transformWorld) };
		//	XMStoreFloat4x4(&data->InvWorld, inverseWorld);

		//	xmmat world{ XMLoadFloat4x4(&data->World) };
		//	xmmat wvp{ XMMatrixMultiply(world, frameInfo.Camera->ViewProjection()) };
		//	XMStoreFloat4x4(&data.WorldViewProjection, wvp);

		//	const MaterialSurface* const surface{ materialsCache.MaterialSurfaces[i] };
		//	memcpy(&data.BaseColor, surface, sizeof(MaterialSurface));

		//	currentDataPointer = cbuffer.AllocateSpace<hlsl::PerObjectData>();
		//	memcpy(currentDataPointer, &data, sizeof(hlsl::PerObjectData));
		//}
	}
};
REGISTER_SYSTEM(PreparePerObjectDataSystem, ecs::system::SystemGroup::PostUpdate, 1);

}