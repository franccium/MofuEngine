#pragma once
#include "CommonHeaders.h"

namespace mofu::graphics {
DEFINE_TYPED_ID(camera_id)
class Camera
{
public:
	enum Type : u32
	{
		Perspective,
		Orthographic,
	};

	constexpr explicit Camera(camera_id id) : _id{ id } {}
	constexpr Camera() = default;

	constexpr camera_id GetID() const { return _id; }
	constexpr bool IsValid() const { return id::IsValid(_id); }

	void Up(v3 up) const;
	void FieldOfView(f32 fov) const;
	void AspectRatio(f32 aspectRatio) const;
	void ViewWidth(f32 width) const;
	void ViewHeight(f32 height) const;
	void Range(f32 nearZ, f32 farZ) const;

	m4x4 View() const;
	m4x4 Projection() const;
	m4x4 InvProjection() const;
	m4x4 ViewProjection() const;
	m4x4 InvViewProjection() const;

	v3 Up() const;
	f32 NearZ() const;
	f32 FarZ() const;
	v3 AspectRatio();
	f32 FieldOfView() const;
	f32 ViewWidth() const;
	f32 ViewHeight() const;
	Type ProjectionType();
	id_t EntityID() const;	

private:
	camera_id _id{ id::INVALID_ID };

};
}