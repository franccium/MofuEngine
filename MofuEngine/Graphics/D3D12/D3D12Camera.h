#pragma once
#include "D3D12CommonHeaders.h"
#include "EngineAPI/Camera.h"

namespace mofu::graphics::d3d12::camera {
class D3D12Camera
{
public:
	explicit D3D12Camera(CameraInitInfo info);

	void Update();
	
	void Up(v3 up);
	constexpr void FieldOfView(f32 fov);
	constexpr void AspectRatio(f32 aspectRatio);
	constexpr void ViewWidth(f32 width);
	constexpr void ViewHeight(f32 height);	
	constexpr void NearZ(f32 nearZ);
	constexpr void FarZ(f32 farZ);

	[[nodiscard]] constexpr xmmat View() const { return _view; }
	[[nodiscard]] constexpr xmmat Projection() const { return _projection; }
	[[nodiscard]] constexpr xmmat InverseProjection() const { return _inverseProjection; }
	[[nodiscard]] constexpr xmmat InverseView() const { return _inverseView; }
	[[nodiscard]] constexpr xmmat ViewProjection() const { return _viewProjection; }
	[[nodiscard]] constexpr xmmat PrevViewProjection() const { return _prevViewProjection; }
	[[nodiscard]] constexpr xmmat InverseViewProjection() const { return _inverseViewProjection; }
	[[nodiscard]] constexpr xmm Up() const { return _up; }
	[[nodiscard]] constexpr xmm Position() const { return _position; }
	[[nodiscard]] constexpr xmm Direction() const { return _direction; }
	[[nodiscard]] constexpr f32 NearZ() const { return _nearZ; }
	[[nodiscard]] constexpr f32 FarZ() const { return _farZ; }
	[[nodiscard]] constexpr f32 AspectRatio() const { return _aspectRatio; }
	[[nodiscard]] constexpr f32 ViewWidth() const { return _viewWidth; }
	[[nodiscard]] constexpr f32 ViewHeight() const { return _viewHeight; }
	[[nodiscard]] constexpr f32 FieldOfView() const { return _fieldOfView; }
	[[nodiscard]] constexpr graphics::Camera::Type ProjectionType() const { return _projectionType; }
	[[nodiscard]] constexpr id_t EntityID() const { return _entityID; }

private:
	xmmat _view;
	xmmat _inverseView;
	xmmat _projection;
	xmmat _inverseProjection;
	xmmat _viewProjection;
	xmmat _prevViewProjection;
	xmmat _inverseViewProjection;
	xmm _up;
	xmm _position;
	xmm _direction;
	f32 _nearZ;
	f32 _farZ;
	f32 _aspectRatio;
	f32 _viewWidth;
	f32 _viewHeight;
	f32 _fieldOfView;
	graphics::Camera::Type _projectionType;
	id_t _entityID{ id::INVALID_ID };
};

graphics::Camera CreateCamera(CameraInitInfo info);
void RemoveCamera(camera_id id);
void SetProperty(camera_id id, CameraProperty::Property property, const void* const data, u32 size);
void GetProperty(camera_id id, CameraProperty::Property property, void* const data, u32 size);
[[nodiscard]] D3D12Camera& GetCamera(camera_id id);

}