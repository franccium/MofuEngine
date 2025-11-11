#pragma once
#include "../D3D12CommonHeaders.h"

#if IS_DLSS_ENABLED
#include "../D3D12Surface.h"

namespace mofu::graphics::d3d12 {
	namespace camera {
		class D3D12Camera;
	}
}
namespace mofu::graphics::d3d12::dlss {
constexpr u32 DLSS_OPTIMAL_RESOLUTION_X{ 1067 }; // FIXME: because it initializes later, i cant use it in sampler lod bias calculation

bool Initialize();
bool CreateDLSSFeature();
void Shutdown();
void SetTargetResolution(u32v2 targetResolution);

void DoDLSSPass(DXResource* colorBuffer, DXResource* outputBuffer, const camera::D3D12Camera& camera, DXGraphicsCommandList* const cmdList);
u32v2 GetOptimalResolution();
void ApplyBarriersBeforeDLSS(d3dx::D3D12ResourceBarrierList& barriers);
void ApplyBarriersAfterDLSS(d3dx::D3D12ResourceBarrierList& barriers);

DescriptorHandle GetOutputBufferSRV();
}
#endif