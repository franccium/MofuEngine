#include "D3D12Camera.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/ECSCommon.h"
#include "ECS/Transform.h"

namespace mofu::graphics::d3d12::camera {
namespace {
util::FreeList<D3D12Camera> cameras;

static void SetUpVector(D3D12Camera& camera, const void* const data)
{
    v3 upVector{ *(v3*)data };
    camera.Up(upVector);
}

constexpr static void SetFieldOfView(D3D12Camera& camera, const void* const data)
{
    f32 value{ *(f32*)data };
    camera.FieldOfView(value);
}

constexpr static void SetAspectRatio(D3D12Camera& camera, const void* const data)
{
    f32 value{ *(f32*)data };
    camera.AspectRatio(value);
}

constexpr static void SetViewWidth(D3D12Camera& camera, const void* const data)
{
    f32 value{ *(f32*)data };
    camera.ViewWidth(value);
}

constexpr static void SetViewHeight(D3D12Camera& camera, const void* const data)
{
    f32 value{ *(f32*)data };
    camera.ViewHeight(value);
}

constexpr static void SetNearZ(D3D12Camera& camera, const void* const data)
{
    f32 value{ *(f32*)data };
    camera.NearZ(value);
}

constexpr static void SetFarZ(D3D12Camera& camera, const void* const data)
{
    f32 value{ *(f32*)data };
    camera.FarZ(value);
}

static void GetView(const D3D12Camera& camera, void* const data)
{
    m4x4* const matrix{ (m4x4* const)data };
    DirectX::XMStoreFloat4x4(matrix, camera.View());
}

static void GetProjection(const D3D12Camera& camera, void* const data)
{
    m4x4* const matrix{ (m4x4* const)data };
    DirectX::XMStoreFloat4x4(matrix, camera.Projection());
}

static void GetInverseProjection(const D3D12Camera& camera, void* const data)
{
    m4x4* const matrix{ (m4x4* const)data };
    DirectX::XMStoreFloat4x4(matrix, camera.InverseProjection());
}

static void GetViewProjection(const D3D12Camera& camera, void* const data)
{
    m4x4* const matrix{ (m4x4* const)data };
    DirectX::XMStoreFloat4x4(matrix, camera.ViewProjection());
}

static void GetInverseViewProjection(const D3D12Camera& camera, void* const data)
{
    m4x4* const matrix{ (m4x4* const)data };
    DirectX::XMStoreFloat4x4(matrix, camera.InverseViewProjection());
}

static void GetUpVector(const D3D12Camera& camera, void* const data)
{
    v3* const upVector{ (v3* const)data };
    DirectX::XMStoreFloat3(upVector, camera.Up());
}

constexpr static void GetFieldOfView(const D3D12Camera& camera, void* const data)
{
    f32* const value{ (f32* const)data };
    *value = camera.FieldOfView();
}

constexpr static void GetAspectRatio(const D3D12Camera& camera, void* const data)
{
    f32* const value{ (f32* const)data };
    *value = camera.AspectRatio();
}

constexpr static void GetViewWidth(const D3D12Camera& camera, void* const data)
{
    f32* const value{ (f32* const)data };
    *value = camera.ViewWidth();
}

constexpr static void GetViewHeight(const D3D12Camera& camera, void* const data)
{
    f32* const value{ (f32* const)data };
    *value = camera.ViewHeight();
}

constexpr static void GetNearZ(const D3D12Camera& camera, void* const data)
{
    f32* const value{ (f32* const)data };
    *value = camera.NearZ();
}

constexpr static void GetFarZ(const D3D12Camera& camera, void* const data)
{
    f32* const value{ (f32* const)data };
    *value = camera.FarZ();
}

constexpr static void GetProjectionType(const D3D12Camera& camera, void* const data)
{
    graphics::Camera::Type* const type{ (graphics::Camera::Type* const)data };
    *type = camera.ProjectionType();
}

constexpr static void GetEntityId(const D3D12Camera& camera, void* const data)
{
    id_t* const entityId{ (id_t* const)data };
    *entityId = camera.EntityID();
}

constexpr static void DSet(D3D12Camera&, const void* const) {}

using SetFunction = void(*)(D3D12Camera&, const void* const);
using GetFunction = void(*)(const D3D12Camera&, void* const);
constexpr SetFunction SET[CameraProperty::Count]
{
    SetUpVector,
    SetFieldOfView,
    SetAspectRatio,
    SetViewWidth,
    SetViewHeight,
    SetNearZ,
    SetFarZ,
    DSet,
    DSet,
    DSet,
    DSet,
    DSet,
    DSet,
    DSet
};
constexpr GetFunction GET[CameraProperty::Count]
{
    GetUpVector,
    GetFieldOfView,
    GetAspectRatio,
    GetViewWidth,
    GetViewHeight,
    GetNearZ,
    GetFarZ,
    GetView,
    GetProjection,
    GetInverseProjection,
    GetViewProjection,
    GetInverseViewProjection,
    GetProjectionType,
    GetEntityId
};

} // anonymous namespace

D3D12Camera::D3D12Camera(CameraInitInfo info)
    : _up{ DirectX::XMLoadFloat3(&info.Up) },
	_aspectRatio{ info.AspectRatio },
	_viewWidth{ info.ViewWidth },
	_viewHeight{ info.ViewHeight },
	_fieldOfView{ info.FieldOfView },
	_projectionType{ info.Type },
    _entityID{ info.EntityID },
	_nearZ{ info.NearZ },
	_farZ{ info.FarZ },
	_isDirty{ true }
{
    assert(id::IsValid(_entityID));

    v3 initialPos{ 0.f, -10.f, 0.f };
	_position = DirectX::XMLoadFloat3(&initialPos);
	v3 initialDir{ 0.f, 0.f, 1.f };
	_direction = DirectX::XMLoadFloat3(&initialDir);    

    Update();
}

void 
D3D12Camera::Update()
{
	using namespace DirectX;
    //TODO: take this out of there
    ecs::component::LocalTransform& lt = ecs::scene::GetComponent<ecs::component::LocalTransform>(ecs::Entity{ _entityID });
    _position = XMLoadFloat3(&lt.Position);
    _direction = XMLoadFloat3(&lt.Forward);

	_view = XMMatrixLookToRH(_position, _direction, _up);
    if (_isDirty)
    {
        // NOTE: here _far_z and _near_z are swapped, because we are using reversed depth
		_projection = (_projectionType == graphics::Camera::Type::Perspective) ?
			XMMatrixPerspectiveFovRH(_fieldOfView * XM_PI, _aspectRatio, _farZ, _nearZ) :
			XMMatrixOrthographicRH(_viewWidth, _viewHeight, _farZ, _nearZ);
        _inverseProjection = XMMatrixInverse(nullptr, _projection);
        _isDirty = false;
    }
	_viewProjection = XMMatrixMultiply(_view, _projection);
	_inverseViewProjection = XMMatrixInverse(nullptr, _viewProjection);
}

void 
D3D12Camera::Up(v3 up)
{
	_up = DirectX::XMLoadFloat3(&up);
	_isDirty = true;
}

constexpr void 
D3D12Camera::FieldOfView(f32 fov)
{
	_fieldOfView = fov;
	_isDirty = true;
}

constexpr void 
D3D12Camera::AspectRatio(f32 aspectRatio)
{
	_aspectRatio = aspectRatio;
	_isDirty = true;
}

constexpr void 
D3D12Camera::ViewWidth(f32 width)
{
	_viewWidth = width;
	_isDirty = true;
}

constexpr void 
D3D12Camera::ViewHeight(f32 height)
{
	_viewHeight = height;
	_isDirty = true;
}

constexpr void 
D3D12Camera::NearZ(f32 nearZ)
{
	_nearZ = nearZ;
	_isDirty = true;    
}

constexpr void 
D3D12Camera::FarZ(f32 farZ)
{
    _farZ = farZ;
	_isDirty = true;
}

graphics::Camera 
CreateCamera(CameraInitInfo info)
{
    return graphics::Camera{ camera_id{cameras.add(info)} };
}

void 
RemoveCamera(camera_id id)
{
	assert(id::IsValid(id));
	cameras.remove(id);
}

void 
SetProperty(camera_id id, CameraProperty::Property property, const void* const data, u32 size)
{
	assert(data && size);
	SET[property](GetCamera(id), data);
}

void 
GetProperty(camera_id id, CameraProperty::Property property, void* const data, u32 size)
{
    assert(data && size);
	GET[property](GetCamera(id), data);
}

D3D12Camera& 
GetCamera(camera_id id)
{
    assert(id::IsValid(id));
    return cameras[id];
}

}