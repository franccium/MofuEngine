#include "D3D12Light.h"
#include "Graphics/Lights/Light.h"
#include <span>

namespace mofu::graphics::d3d12::light {
namespace {

class D3D12LightBuffer
{
public:
	struct LightBuffer
	{
		enum Type : u8
		{
			NonCullableLights,
			CullableLights,
			CullingInfos,
			BoundingSpheres,

			Count
		};

		D3D12Buffer Buffer;
		u8* CpuAddress{ nullptr };
	};

	void UpdateLightBuffers(const graphics::light::LightSet& set, u32 lightSetIdx, u32 frameIndex)
	{
		// Non-Cullable
		const u32 nonCullableLightCount{ (u32)set.FirstDisabledNonCullableIndex };
		//D3D12LightSet& lightSet{ lightSets[lightSetIdx] };
		LightBuffer& ncBuffer{ _lightBuffers[LightBuffer::NonCullableLights] };
		//const u32 currentNCLightCount{ lightSet.NonCullableLights.size() };
		const u32 currentNCLightCount{ CurrentNonCullableLightCount };
		if (nonCullableLightCount > currentNCLightCount)
		{
			// resize the buffer
			const u32 neededBufferSize{ nonCullableLightCount * sizeof(DirectionalLightParameters) };
			ResizeBuffer(LightBuffer::NonCullableLights, neededBufferSize, frameIndex);
		}

		// Update
		DirectionalLightParameters* const ncLights{ (DirectionalLightParameters* const)ncBuffer.CpuAddress };
		for (u32 i{ 0 }; i < set.FirstDisabledNonCullableIndex; ++i)
		{
			ncLights[i] = set.NonCullableLights[i];
		}
		CurrentNonCullableLightCount = nonCullableLightCount;

		// Cullable
		const u32 cullableLightCount{ set.FirstDisabledCullableIndex };
		//const u32 currentCLightCount{ lightSet.CullableLights.size() };
		const u32 currentCLightCount{ CurrentCullableLightCount };
		LightBuffer& cullableBuffer{ _lightBuffers[LightBuffer::CullableLights] };
		LightBuffer& infoBuffer{ _lightBuffers[LightBuffer::CullingInfos] };
		LightBuffer& sphereBuffer{ _lightBuffers[LightBuffer::BoundingSpheres] };
		bool buffersResized{ false };
		const u32 neededLightBufferSize{ cullableLightCount * sizeof(CullableLightParameters) };
		const u32 neededInfoBufferSize{ cullableLightCount * sizeof(CullingInfo) };
		const u32 neededSpheresBufferSize{ cullableLightCount * sizeof(Sphere) };
		if (cullableLightCount > currentCLightCount)
		{
			// NOTE: creating the buffer at 1,5x the needed size to avoid recreating them
			ResizeBuffer(LightBuffer::CullableLights, (neededLightBufferSize * 3) >> 1, frameIndex);
			ResizeBuffer(LightBuffer::CullingInfos, (neededInfoBufferSize * 3) >> 1, frameIndex);
			ResizeBuffer(LightBuffer::BoundingSpheres, (neededSpheresBufferSize * 3) >> 1, frameIndex);
			buffersResized = true;
		}
		CurrentCullableLightCount = cullableLightCount;

		// Update
		//std::span<CullableLightParameters> readyCullable{ set.CullableLights.data(), set.CullableLights.size() - set.FirstDisabledCullableIndex };
		if (buffersResized || currentLightSetIndex != lightSetIdx)
		{
			// copy the whole light data
			//NOTE: copies only non-disabled lights
			memcpy(cullableBuffer.CpuAddress, set.CullableLights.data(), cullableLightCount);
			memcpy(infoBuffer.CpuAddress, set.CullingInfos.data(), cullableLightCount);
			memcpy(sphereBuffer.CpuAddress, set.BoundingSpheres.data(), cullableLightCount);

			currentLightSetIndex = lightSetIdx;
		}
		else
		{
			// update dirty lights
			for (u32 i{ set.FirstDirtyCullableIndex }; i < set.FirstDisabledCullableIndex; ++i)
			{
				assert(i * sizeof(CullableLightParameters) < neededLightBufferSize);
				assert(i * sizeof(CullingInfo) < neededInfoBufferSize);
				assert(i * sizeof(Sphere) < neededSpheresBufferSize);

				memcpy(cullableBuffer.CpuAddress + i * sizeof(CullableLightParameters), &set.CullableLights[i], sizeof(CullableLightParameters));
				memcpy(infoBuffer.CpuAddress + i * sizeof(CullingInfo), &set.CullingInfos[i], sizeof(CullingInfo));
				memcpy(sphereBuffer.CpuAddress + i * sizeof(Sphere), &set.BoundingSpheres[i], sizeof(Sphere));
			}
		}
	}

	constexpr D3D12_GPU_VIRTUAL_ADDRESS GetNonCullableLights() const { return _lightBuffers[LightBuffer::NonCullableLights].Buffer.GpuAddress(); }
	constexpr D3D12_GPU_VIRTUAL_ADDRESS GetCullableLights() const { return _lightBuffers[LightBuffer::CullableLights].Buffer.GpuAddress(); }
	constexpr D3D12_GPU_VIRTUAL_ADDRESS GetCullingInfos() const { return _lightBuffers[LightBuffer::CullingInfos].Buffer.GpuAddress(); }
	constexpr D3D12_GPU_VIRTUAL_ADDRESS GetBoundingSpheres() const { return _lightBuffers[LightBuffer::BoundingSpheres].Buffer.GpuAddress(); }

	void ResizeBuffer(LightBuffer::Type type, u32 newSize, [[maybe_unused]] u32 frameIndex)
	{
		LightBuffer& lightBuffer{ _lightBuffers[type] };
		if (newSize == 0 || lightBuffer.Buffer.Size() >= math::AlignUp<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(newSize)) return;

		lightBuffer.Buffer = D3D12Buffer{ ConstantBuffer::DefaultInitInfo(newSize), true };

		NAME_D3D12_OBJECT_INDEXED(lightBuffer.Buffer.Buffer(), frameIndex,
			type == LightBuffer::NonCullableLights ? L"Non-cullable Light Buffer" :
			type == LightBuffer::CullableLights ? L"Cullable Light Buffer" :
			type == LightBuffer::CullingInfos ? L"Culling Info Light Buffer" : L"Bounding Spheres Light Buffer");

		D3D12_RANGE range{};
		DXCall(lightBuffer.Buffer.Buffer()->Map(0, &range, (void**)&lightBuffer.CpuAddress));
		assert(lightBuffer.CpuAddress);
	}

	void Release()
	{
		for (u32 i{ 0 }; i < LightBuffer::Count; ++i)
		{
			_lightBuffers[i].Buffer.Release();
			_lightBuffers[i].CpuAddress = nullptr;
		}
	}

	u32 CurrentCullableLightCount{ 0 };
	u32 CurrentNonCullableLightCount{ 0 };
private:
	LightBuffer _lightBuffers[LightBuffer::Count]{};
	u32 currentLightSetIndex{ 0 };
};

Vec<D3D12LightSet> lightSets{};
D3D12LightBuffer lightBuffers[FRAME_BUFFER_COUNT]{};

} // anonymous namespace

bool
Initialize()
{
	return true;
}

void 
Shutdown()
{
	for (u32 i{ 0 }; i < FRAME_BUFFER_COUNT; ++i)
	{
		lightBuffers[i].Release();
	}
}

void 
CreateLightSet()
{
}

void 
RemoveLightSet()
{
}

void 
UpdateLightBuffers(const graphics::light::LightSet& set, u32 lightSetIdx, u32 frameIndex)
{
	lightBuffers[frameIndex].UpdateLightBuffers(set, lightSetIdx, frameIndex);
}

u32 
GetDirectionalLightsCount(u32 frameIndex)
{
	return lightBuffers[frameIndex].CurrentNonCullableLightCount;
}

D3D12_GPU_VIRTUAL_ADDRESS 
GetNonCullableLightBuffer(u32 frameIndex)
{
	return lightBuffers[frameIndex].GetNonCullableLights();
}

D3D12_GPU_VIRTUAL_ADDRESS 
GetCullableLightBuffer(u32 frameIndex)
{
	return lightBuffers[frameIndex].GetCullableLights();
}

D3D12_GPU_VIRTUAL_ADDRESS
GetCullingInfoBuffer(u32 frameIndex)
{
	return lightBuffers[frameIndex].GetCullingInfos();
}

D3D12_GPU_VIRTUAL_ADDRESS
GetBoundingSpheresBuffer(u32 frameIndex)
{
	return lightBuffers[frameIndex].GetBoundingSpheres();
}

}