#pragma once
#ifdef _WIN64
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#ifndef _ALWAYS_INLINE
#if defined(__GNUC__)
#define _ALWAYS_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define _ALWAYS_INLINE __forceinline
#else
#define _ALWAYS_INLINE
#endif
#endif

#ifndef _NO_INLINE
#if defined(__GNUC__)
#define _NO_INLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define _NO_INLINE __declspec(noinline)
#else
#define _NO_INLINE
#endif
#endif

#define TODO_(x) assert(false && x)