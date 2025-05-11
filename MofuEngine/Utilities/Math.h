#pragma once
#include "MathTypes.h"

namespace mofu::math {

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