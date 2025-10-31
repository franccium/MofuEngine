#pragma once
#include <Jolt/Jolt.h>
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
constexpr float SQRT2 = 1.41421356f;
constexpr float SQRT3 = 1.73205080f;
constexpr float SQRT12 = 0.70710678f;
constexpr float SQRT13 = 0.57735026f;

}

namespace mofu {
#ifdef _WIN64
using v2 = DirectX::XMFLOAT2;
using v2a = DirectX::XMFLOAT2A;
//using v3 = DirectX::XMFLOAT3;
struct v3 : public DirectX::XMFLOAT3
{
    constexpr v3() = default;
    constexpr v3(const DirectX::XMFLOAT3& other) : DirectX::XMFLOAT3(other) {}
    constexpr v3(float x, float y, float z) : DirectX::XMFLOAT3(x, y, z) {}

    operator JPH::Float3() const 
    {
        return JPH::Float3(x, y, z);
    }

    JPH::Vec3 AsJPVec3() const 
    {
        return JPH::Vec3(x, y, z);
    }

    v3(const JPH::Float3& other) 
    {
        x = other.x;
        y = other.y;
        z = other.z;
    }

    v3& operator=(const JPH::Float3& other) 
    {
        x = other.x;
        y = other.y;
        z = other.z;
        return *this;
    }
};
using v3a = DirectX::XMFLOAT3A;
using v4 = DirectX::XMFLOAT4;
using v4a = DirectX::XMFLOAT4A;
//using quat = v4;
struct quat : public DirectX::XMFLOAT4 
{
    constexpr quat() = default;
    constexpr quat(const DirectX::XMFLOAT4& other) : DirectX::XMFLOAT4(other) {}
    constexpr quat(float x, float y, float z, float w) : DirectX::XMFLOAT4(x, y, z, w) {}

    operator JPH::Quat() const 
    {
        return JPH::Quat(x, y, z, w);
    }

    quat(const JPH::Quat& other) 
    {
        x = other.GetX();
        y = other.GetY();
        z = other.GetZ();
        w = other.GetW();
    }

    quat& operator=(const JPH::Quat& other) 
    {
        x = other.GetX();
        y = other.GetY();
        z = other.GetZ();
        w = other.GetW();
        return *this;
    }
};

using u32v2 = DirectX::XMUINT2;
using u32v3 = DirectX::XMUINT3;
using u32v4 = DirectX::XMUINT4;

using i32v2 = DirectX::XMINT2;
using i32v3 = DirectX::XMINT3;
using i32v4 = DirectX::XMINT4;

using m3x3 = DirectX::XMFLOAT3X3;
using m3x4 = DirectX::XMFLOAT3X4;
using m3x4a = DirectX::XMFLOAT3X4A;
using m4x3 = DirectX::XMFLOAT4X3;
using m4x3a = DirectX::XMFLOAT4X3A;
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
constexpr v3 v3forward{ 0.f, 0.f, -1.f };

constexpr quat quatIndentity{ 0.f, 0.f, 0.f, 1.f };

struct OBB
{

};

}