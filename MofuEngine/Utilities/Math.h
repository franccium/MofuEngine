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

template<u32 bits>
[[nodiscard]] constexpr u32
PackUnitFloat(f32 val)
{
	static_assert(bits && bits <= sizeof(f32) * 8);
	assert(val >= 0.f && val <= 1.f);
	constexpr f32 intervals{ (f32)(1u << bits) - 1.f };
	return (u32)(val * intervals + 0.5f);
}

template<u32 bits>
[[nodiscard]] constexpr f32
UnpackToUnitFloat(u32 i)
{
	static_assert(bits && bits <= sizeof(f32) * 8);
	assert(i <= (1u << bits));
	constexpr f32 intervals{ (f32)(1u << bits - 1) };
	return (f32)i / intervals;
}

template<u32 bits>
[[nodiscard]] constexpr f32
UnpackToFloat(u32 i, f32 min, f32 max)
{
	assert(min <= max);
	return UnpackToUnitFloat<bits>(i) * (max - min) + min;
}

template<u32 bits>
[[nodiscard]] constexpr u32
PackFloat(f32 val, f32 min, f32 max)
{
	assert(min <= max);
	assert(val <= max && val >= min);
	const f32 distance{ (val - min) / (max - min) };
	return PackUnitFloat<bits>(distance);
}

// NOTE: ignores any bytes that don't fit in an 8-byte alignment, the data should be aligned size-up and filled with zeros first
[[nodiscard]] constexpr u64
CRC32_u64(const u8* const data, u64 size)
{
	assert(size >= sizeof(u64));
	assert(data && size);
	u64 crc{ 0 };
	const u8* at{ data };
	const u8* const end{ data + AlignDown<sizeof(u64)>(size) };
	while (at < end)
	{
		crc = _mm_crc32_u64(crc, *((const u64*)at));
		at += sizeof(u64);
	}
	return crc;
}

[[nodiscard]] inline u8
log2(u64 value)
{
	unsigned long mssb;
	unsigned long lssb;

	if (_BitScanReverse64(&mssb, value) > 0 && _BitScanForward64(&lssb, value) > 0)
		return u8(mssb + (mssb == lssb ? 0 : 1));
	else
		return 0;
}

[[nodiscard]] inline bool
IsEqual(f32 a, f32 b)
{
	return abs(a - b) < math::EPSILON;
}

using namespace DirectX;
[[nodiscard]] inline v3 
QuatToEulerDeg(const quat& quat)
{
	xmm q{ XMLoadFloat4(&quat) };
	v3a euler{};

	m4x4a rotMat{};
	XMStoreFloat4x4A(&rotMat, XMMatrixRotationQuaternion(q));
	euler.y = asinf(-rotMat._31); // Pitch
	if (cosf(euler.y) > 0.0001f) 
	{
		euler.x = atan2f(rotMat._32, rotMat._33); // Yaw
		euler.z = atan2f(rotMat._21, rotMat._11); // Roll
	}
	else 
	{
		euler.x = 0.0f;
		euler.z = atan2f(-rotMat._12, rotMat._22);
	}

	euler.x = XMConvertToDegrees(euler.x);
	euler.y = XMConvertToDegrees(euler.y);
	euler.z = XMConvertToDegrees(euler.z);

	return euler;
}

[[nodiscard]] inline v3
QuatToEulerRad(const quat& quat)
{
	xmm q{ XMLoadFloat4(&quat) };
	v3a euler{};

	m4x4a rotMat{};
	XMStoreFloat4x4A(&rotMat, XMMatrixRotationQuaternion(q));
	euler.y = asinf(-rotMat._31); // Pitch
	if (cosf(euler.y) > 0.0001f)
	{
		euler.x = atan2f(rotMat._32, rotMat._33); // Yaw
		euler.z = atan2f(rotMat._21, rotMat._11); // Roll
	}
	else
	{
		euler.x = 0.0f;
		euler.z = atan2f(-rotMat._12, rotMat._22);
	}

	return euler;
}

[[nodiscard]] inline quat 
EulerDegToQuat(const v3& eulerDeg)
{
	v4a q{};
	XMStoreFloat4A(&q, XMQuaternionRotationRollPitchYaw(XMConvertToRadians(eulerDeg.x), XMConvertToRadians(eulerDeg.y), XMConvertToRadians(eulerDeg.z)));
	return q;
}

[[nodiscard]] 
inline quat EulerRadToQuat(const v3& eulerRad)
{
	v4a q{};
	XMStoreFloat4A(&q, XMQuaternionRotationRollPitchYaw(eulerRad.x, eulerRad.y, eulerRad.z));
	return q;
}
}