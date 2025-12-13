#include "D3D12Transpacency.h"
#include "Graphics/GraphicsTypes.h"

namespace mofu::graphics::d3d12::transparency {
namespace {
UAVClearableBuffer _transparencyHeadBuffer;
UAVClearableBuffer _transparencyListBuffer;
constexpr f32 CLEAR_VALUE[4]{ 0.f, 0.f, 0.f, 0.f };
} // anonymous namespace

bool 
Initialize()
{
	CreateBuffers({1, 1});

	return true;
}

void
CreateBuffers(u32v2 renderDimensions)
{
	_transparencyHeadBuffer.Release();
	_transparencyListBuffer.Release();

	u32 size{ sizeof(u32) * renderDimensions.x * renderDimensions.y };
	D3D12BufferInitInfo info{ UAVClearableBuffer::GetDefaultInitInfo(size) };
	_transparencyHeadBuffer = UAVClearableBuffer{ info };
	NAME_D3D12_OBJECT_INDEXED(_transparencyHeadBuffer.Buffer(), size, L"Transparency List Head Buffer");
	info.size = sizeof(hlsl::particles::ParticleTransparencyNodePacked) * renderDimensions.x * renderDimensions.y * MAX_TRANSPARENCY_LAYERS;
	_transparencyListBuffer = UAVClearableBuffer{ info };
	NAME_D3D12_OBJECT_INDEXED(_transparencyListBuffer.Buffer(), info.size, L"Transparency List Buffer");
}

void 
Shutdown()
{

}

void 
AddBarriersForRendering(d3dx::D3D12ResourceBarrierList& barriers)
{
	barriers.AddTransitionBarrier(_transparencyHeadBuffer.Buffer(), 
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	barriers.AddTransitionBarrier(_transparencyListBuffer.Buffer(), 
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void 
AddBarriersForReading(d3dx::D3D12ResourceBarrierList& barriers)
{
	barriers.AddTransitionBarrier(_transparencyHeadBuffer.Buffer(), 
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers.AddTransitionBarrier(_transparencyListBuffer.Buffer(), 
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void 
ClearBuffers(DXGraphicsCommandList* const cmdList)
{
	_transparencyHeadBuffer.ClearUAV(cmdList, CLEAR_VALUE);
	_transparencyListBuffer.ClearUAV(cmdList, CLEAR_VALUE);
}

D3D12_GPU_VIRTUAL_ADDRESS GetTransparencyHeadBufferGPUAddr()
{
	return _transparencyHeadBuffer.GpuAddress();
}

D3D12_GPU_VIRTUAL_ADDRESS GetTransparencyListGPUAddr()
{
	return _transparencyListBuffer.GpuAddress();
}

u32 GetTransparencyHeadBufferIndex()
{
	return _transparencyHeadBuffer.UAV().index;
}

u32 GetTransparencyListIndex()
{
	return _transparencyListBuffer.UAV().index;
}

}