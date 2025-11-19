#pragma once
#include "CommonDefines.h"
#include "Graphics/D3D12/Shaders/CommonDefines.hlsli"
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
#define RENDER_GUI 1
#define SHADER_HOT_RELOAD_ENABLED 1
#define PHYSICS_DEBUG_RENDER_ENABLED 1

static_assert(!(RAYTRACING && (PATHTRACE_MAIN&& PATHTRACE_SHADOWS)), "Path tracing cannot be enabled with both main and shadow raytracing at the same time");