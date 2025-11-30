#pragma once
#include "D3D12CommonHeaders.h"	
#include "D3D12Surface.h"

namespace mofu::graphics::d3d12 {
	struct D3D12FrameInfo;
}

namespace mofu::graphics::d3d12::resolve {
bool Initialize();
void Shutdown();
void SetBufferSize(u32v2 size);
void AddTransitionsPreResolve(d3dx::D3D12ResourceBarrierList& barriers);
void AddTransitionsPostResolve(d3dx::D3D12ResourceBarrierList& barriers);
void ResolvePass(DXGraphicsCommandList* cmdList, const D3D12FrameInfo& frameInfo, D3D12_CPU_DESCRIPTOR_HANDLE rtv, const D3D12Surface& surface);
u32 GetResolveOutputSRV();
DXResource* GetResolveOutputBuffer();
void ResetShaders();
}