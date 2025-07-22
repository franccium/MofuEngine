#include "D3D12LightCulling.h"
#include "Graphics/GraphicsTypes.h"

namespace mofu::graphics::d3d12::light {
namespace {
struct LightCullingRootParameter 
{
	enum Parameter : u32
	{
		GlobalShaderData,
		Constants,
		FrustumsOutOrIndexCounted, // either attached as a UAV for the frustum buffer or a UAV for index counter during light culling
		FrustumsIn,
		CullingInfo,
		BoundingSpheres,
		LightGridOpaque,
		LightIndexListOpaque,

		Count
	};
};

struct CullingParameters
{
	D3D12Buffer Frustums;
	D3D12Buffer LightGridAndIndexList; // the light index list buffer is offset within it, pointed to by LightIndexListOpaqueBuffer
	UAVClearableBuffer LightIndexCounter;
	hlsl::LightCullingDispatchParameters GridFrustumsDispatchParams;
	hlsl::LightCullingDispatchParameters LightCullingDispatchParams;
	u32 FrustumCount{ 0 };
	u32 ViewWidth{ 0 };
	u32 ViewHeight{ 0 };
	f32 CameraFOV{ 0.f };
	D3D12_GPU_VIRTUAL_ADDRESS LightIndexListOpaqueBuffer{ 0 };
	bool HasLights{ true };
};

// NOTE: maximum for the average number of lights per tile
constexpr u32 MAX_LIGHTS_PER_TILE{ 256 };

ID3D12RootSignature* LightCullingRootSignature{ nullptr };
ID3D12PipelineState* GridFrustumsPSO{ nullptr };
ID3D12PipelineState* LightCullingPSO{ nullptr };

} // anonymous namespace

void
CullLights(DXGraphicsCommandList* const cmdList, const D3D12FrameInfo& d3d12FrameInfo, d3dx::D3D12ResourceBarrierList& barriers)
{

}

id_t 
AddLightCuller()
{
	return 0;
}

void 
RemoveLightCuller(id_t cullerID)
{

}

D3D12_GPU_VIRTUAL_ADDRESS
GetLightGridOpaque(u32 lightCullingID, u32 frameIndex)
{
	return D3D12_GPU_VIRTUAL_ADDRESS();
}

D3D12_GPU_VIRTUAL_ADDRESS 
GetLightIndexListOpaque(u32 lightCullingID, u32 frameIndex)
{
	return D3D12_GPU_VIRTUAL_ADDRESS();
}

}
