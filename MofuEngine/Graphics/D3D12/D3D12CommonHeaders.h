#pragma once
#include "CommonHeaders.h"
#include "Graphics/Renderer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxcompiler.lib")

// COM call to D3D API succeeded assert
#ifdef _DEBUG
#ifndef DXCall
#define DXCall(x)\
if(FAILED(x)) {\
	char line_number[32];\
	sprintf_s(line_number, "%u", __LINE__);\
	OutputDebugStringA("Error in: ");\
	OutputDebugStringA(__FILE__);\
	OutputDebugStringA("\nline: ");\
	OutputDebugStringA(line_number);\
	OutputDebugStringA("\n");\
	OutputDebugStringA(#x);\
	OutputDebugStringA("\n");\
	__debugbreak();\
}
#endif
#else
#ifndef DXCall
#define DXCall(x) x
#endif
#endif

#ifdef _DEBUG
#define NAME_D3D12_OBJECT(obj, name) obj->SetName(name); OutputDebugString(L"*** D3D12 Object Created: "); OutputDebugString(name); OutputDebugString(L" ***\n");
#define NAME_D3D12_OBJECT_INDEXED(obj, index, name)\
{\
wchar_t full_name[128];\
if (swprintf_s(full_name, L"%s[%llu]", name, (u64)index) > 0)\
{\
	obj->SetName(full_name);\
	OutputDebugString(L"*** D3D12 Object Created: "); OutputDebugString(full_name); OutputDebugString(L" ***\n");\
}\
}
#else
#define NAME_D3D12_OBJECT(x, name)
#define NAME_D3D12_OBJECT_INDEXED(obj, index, name)
#endif

using DXDevice = ID3D12Device10;
using DXGraphicsCommandList = ID3D12GraphicsCommandList7;
using DXHeap = ID3D12Heap1;
using DXResource = ID3D12Resource2;
using DXDebug = ID3D12Debug6;

using DXSwapChain = IDXGISwapChain4;
using DXAdapter = IDXGIAdapter4;
using DXGIFactory = IDXGIFactory7;

namespace mofu::graphics::d3d12 {

constexpr u32 FRAME_BUFFER_COUNT{ 3 };

#define MT 1

#if MT

constexpr u32 DEPTH_WORKERS{ 3 };
constexpr u32 DEPTH_SETUP_LIST{ 0 };
constexpr u32 FIRST_DEPTH_WORKER{ DEPTH_WORKERS == 1 ? 0 : 1 };

constexpr u32 MAIN_SETUP_LIST{ DEPTH_WORKERS + FIRST_DEPTH_WORKER };
constexpr u32 FIRST_MAIN_GPASS_WORKER{ MAIN_SETUP_LIST + 1 };
constexpr u32 GPASS_WORKERS{ 3 };

constexpr u32 COMMAND_LIST_WORKERS{ DEPTH_WORKERS + GPASS_WORKERS + FIRST_DEPTH_WORKER + 1 + 1 };

constexpr u32 CLOSING_LIST_INDEX{ FIRST_MAIN_GPASS_WORKER + GPASS_WORKERS };
static_assert(CLOSING_LIST_INDEX == COMMAND_LIST_WORKERS - 1);

constexpr u32 BUNDLE_COUNT{ DEPTH_WORKERS + GPASS_WORKERS };

constexpr u32 MAIN_BUNDLE_INDEX{ DEPTH_WORKERS };
#else
constexpr u32 COMMAND_LIST_WORKERS{ 1 };

constexpr u32 DEPTH_WORKERS{ 1 };
constexpr u32 DEPTH_SETUP_LIST{ 0 };
constexpr u32 FIRST_DEPTH_WORKER{ 0 };

constexpr u32 MAIN_SETUP_LIST{ 0 };
constexpr u32 FIRST_MAIN_GPASS_WORKER{ 0 };
constexpr u32 GPASS_WORKERS{ 1 };

constexpr u32 CLOSING_LIST_INDEX{ 0 };

// NOTE: make sure they are actually closed so it doesnt break the reset on renderItemsUpdated
constexpr u32 BUNDLE_COUNT{ 2 };

constexpr u32 MAIN_BUNDLE_INDEX{ 1 };
#endif

}

#include "D3D12Resources.h"
#include "D3D12Helpers.h"

#define RENDER_2D_TEST 0