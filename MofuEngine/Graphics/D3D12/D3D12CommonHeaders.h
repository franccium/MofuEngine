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
using DXGIFactory = IDXGIFactory7;
using DXGraphicsCommandList = ID3D12GraphicsCommandList7;
using DXSwapChain = IDXGISwapChain4;
using DXResource = ID3D12Resource2;
using DXDebug = ID3D12Debug6;
using DXAdapter = IDXGIAdapter4;

#include "D3D12Resources.h"
#include "D3D12Helpers.h"