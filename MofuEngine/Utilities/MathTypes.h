#pragma once
#include "CommonHeaders.h"

#ifdef _WIN64
#include <DirectXMath.h>
#endif

namespace mofu::math {
constexpr float PI = 3.14159265f;
constexpr float TAU = 6.28318548f;
constexpr float HALF_PI = 0.5f * PI;
constexpr float PIDIV4 = 0.25f * PI;
constexpr float INV_PI = 1.f / PI;
constexpr float INV_TAU = 1.f / TAU;
constexpr float EPSILON = 1e-5f;

}

namespace mofu {
#ifdef _WIN64
using v2 = DirectX::XMFLOAT2;
using v2a = DirectX::XMFLOAT2A;
using v3 = DirectX::XMFLOAT3;
using v3a = DirectX::XMFLOAT3A;
using v4 = DirectX::XMFLOAT4;
using v4a = DirectX::XMFLOAT4A;
using quat = v4;

using u32v2 = DirectX::XMUINT2;
using u32v3 = DirectX::XMUINT3;
using u32v4 = DirectX::XMUINT4;

using i32v2 = DirectX::XMINT2;
using i32v3 = DirectX::XMINT3;
using i32v4 = DirectX::XMINT4;

using m3x3 = DirectX::XMFLOAT3X3;
using m4x4 = DirectX::XMFLOAT4X4;
using m4x4a = DirectX::XMFLOAT4X4A;

using xmm = DirectX::XMVECTOR;
using xmmat = DirectX::XMMATRIX;
#endif

constexpr v2 v2zero{ 0.f, 0.f };
constexpr v3 v3zero{ 0.f, 0.f, 0.f };
constexpr v4 v4zero{ 0.f, 0.f, 0.f, 0.f };
constexpr v2 v2one{ 1.f, 1.f };
constexpr v3 v3one{ 1.f, 1.f, 1.f };
constexpr v4 v4one{ 1.f, 1.f, 1.f, 1.f };

constexpr quat quatIndentity{ 0.f, 0.f, 0.f, 1.f };

struct OBB
{

};
}