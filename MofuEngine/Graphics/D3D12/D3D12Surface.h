#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12 {
class D3D12Surface
{
public:
	void Present() const;

	[[nodiscard]] constexpr u32 Width() const { return (u32)_viewport.Width; }
	[[nodiscard]] constexpr u32 Height() const { return (u32)_viewport.Height; }

private:
	D3D12_VIEWPORT _viewport;

};

}