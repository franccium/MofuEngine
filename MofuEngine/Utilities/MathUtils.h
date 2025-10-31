#pragma once
#include "CommonDefines.h"
#include "StandardTypes.h"
#ifdef _WIN64
#include <DirectXMath.h>
#endif

namespace mofu {

#ifdef _WIN64
namespace math {
[[nodiscard]] _ALWAYS_INLINE DirectX::XMFLOAT3
Scale(DirectX::XMFLOAT3 v, f32 scalar)
{
    DirectX::XMVECTOR V{ DirectX::XMLoadFloat3(&v) };
    V = DirectX::XMVectorScale(V, scalar);
    DirectX::XMFLOAT3 res;
    DirectX::XMStoreFloat3(&res, V);
    return res;
}

[[nodiscard]] _ALWAYS_INLINE DirectX::XMFLOAT3
Add(DirectX::XMFLOAT3 v1, DirectX::XMFLOAT3 v2)
{
    DirectX::XMVECTOR V1{ DirectX::XMLoadFloat3(&v1) };
    DirectX::XMVECTOR V2{ DirectX::XMLoadFloat3(&v2) };
    V1 = DirectX::XMVectorAdd(V1, V2);
    DirectX::XMFLOAT3 res;
    DirectX::XMStoreFloat3(&res, V1);
    return res;
}
}

#endif
}