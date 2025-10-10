#pragma once
#include "Graphics/D3D12/D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::hlsl {
using float4x4 = m4x4a;
using float4 = v4;
using float3 = v3;
using float2 = v2;
using uint4 = u32v4;
using uint3 = u32v3;
using uint2 = u32v2;
using uint = u32;
#define row_major 
#include "D3D12/Shaders/CommonTypes.hlsli"
#undef row_major
namespace content {
#include "Content/Shaders/ContentTypes.hlsli"
}
}