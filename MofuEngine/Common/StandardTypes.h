#pragma once
#include <stdint.h>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using f32 = float;

constexpr u64 U64_INVALID_ID{ 0xffffffffffffffff };
constexpr u32 U32_INVALID_ID{ 0xffffffff };
constexpr u16 U16_INVALID_ID{ 0xffff };
constexpr u8 U8_INVALID_ID{ 0xff };