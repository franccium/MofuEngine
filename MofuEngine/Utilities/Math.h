#pragma once
#include "MathTypes.h"

namespace mofu::math {

template<typename T>
[[nodiscard]] constexpr T
Clamp(T val, T min, T max)
{
	assert(min <= max);
	return (val < min) ? min : (val > max) ? max : val;
}

[[nodiscard]] inline float
Clamp(float val, float min, float max)
{
	__m128 v = _mm_set_ss(val);
	__m128 minV = _mm_set_ss(min);
	__m128 maxV = _mm_set_ss(max);
	v = _mm_max_ss(minV, v);
	v = _mm_min_ss(maxV, v);
	return _mm_cvtss_f32(v);
}

[[nodiscard]] inline float
Saturate(float val)
{
	return Clamp(val, 0.0f, 1.0f);
}

template<u64 alignment>
[[nodiscard]] constexpr u64
AlignUp(u64 size)
{
	// <64>(217) --> 0xD9 + 0x3F = 0x118, ~0x3F = 0xC0 --> 0x118 & 0xC0 clears the lower 6 bits == 0x100
	static_assert(alignment, "Alignment must be non-zero");
	constexpr u64 mask{ alignment - 1 };
	static_assert((alignment & mask) == 0, "Alignment must be a power of two");
	return ((size + mask) & ~mask);
}

template<u64 alignment>
[[nodiscard]] constexpr u64
AlignDown(u64 size)
{
	// <64>(217) --> ~0x3F = 0xC0 --> 0xD9 & 0xC0 == 0xC0
	static_assert(alignment, "Alignment must be non-zero");
	constexpr u64 mask{ alignment - 1 };
	static_assert((alignment & mask) == 0, "Alignment must be a power of two");
	return (size & ~mask);
}

[[nodiscard]] constexpr u64
AlignUp(u64 size, u64 alignment)
{
	// <64>(217) --> 0xD9 + 0x3F = 0x118, ~0x3F = 0xC0 --> 0x118 & 0xC0 clears the lower 6 bits == 0x100
	assert(alignment && "Alignment must be non-zero");
	const u64 mask{ alignment - 1 };
	assert((alignment & mask) == 0 && "Alignment must be a power of two");
	return ((size + mask) & ~mask);
}

[[nodiscard]] constexpr u64
AlignDown(u64 size, u64 alignment)
{
	// <64>(217) --> ~0x3F = 0xC0 --> 0xD9 & 0xC0 == 0xC0
	assert(alignment && "Alignment must be non-zero");
	const u64 mask{ alignment - 1 };
	assert((alignment & mask) == 0 && "Alignment must be a power of two");
	return (size & ~mask);
}

}