#pragma once
#include "Math.h"

#ifdef _WIN64

namespace mofu::packing {
[[nodiscard]] inline u32 
Pack2xFloat1(f32 low, f32 high)
{
	const half lowHalf{ DirectX::PackedVector::XMConvertFloatToHalf(low) };
	const half highHalf{ DirectX::PackedVector::XMConvertFloatToHalf(high) };
	return ((u32)lowHalf | (highHalf << 16));
}

inline void
Unpack2xFloat1(u32 packed, f32& x, f32& y)
{
	const u16 lowHalf{ packed & 0xFFFF };
	const u16 highHalf{ (packed >> 16) };
	y = DirectX::PackedVector::XMConvertHalfToFloat(lowHalf);
	x = DirectX::PackedVector::XMConvertHalfToFloat(highHalf);
}

#endif


}