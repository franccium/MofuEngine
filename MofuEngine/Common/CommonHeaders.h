#pragma once
// C/C++
#include <stdint.h>
#include <assert.h>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <mutex>
#include <typeinfo>

// Custom
#include "StandardTypes.h"
#include "Utilities/DataStructures/DataStructures.h"
#include "Utilities/Math.h"
#include "id.h"

#ifdef _DEBUG
#define DEBUG_OP(expr) expr
#ifdef _WIN64 // temporary here
#define DEBUG_LOG(expr) OutputDebugStringA("*** Debug Log: "); OutputDebugStringA(expr)
#define DEBUG_LOGW(expr) OutputDebugStringW(L"*** Debug Log: "); OutputDebugStringW(expr)
#endif
#else
#define DEBUG_OP(expr)
#define DEBUG_LOG(expr)
#define DEBUG_LOGW(expr)
#endif

#ifndef DISABLE_COPY
#define DISABLE_COPY(T)\
	explicit T(const T&) = delete;\
	T& operator=(const T&) = delete;
#endif

#ifndef DISABLE_MOVE
#define DISABLE_MOVE(T)\
	explicit T(T&&) = delete;\
	T& operator=(T&&) = delete;
#endif

#ifndef DISABLE_COPY_AND_MOVE
#define DISABLE_COPY_AND_MOVE(T) DISABLE_COPY(T) DISABLE_MOVE(T)
#endif

#define EDITOR_BUILD 1

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