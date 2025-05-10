#pragma once
#include "CommonHeaders.h"

namespace mofu::platform {
DEFINE_TYPED_ID(window_id);

class Window
{
public:
	constexpr explicit Window(window_id id) : _id{ id } {}
	constexpr Window() = default;
	constexpr window_id GetID() const { return _id; }
	constexpr bool IsValid() const { return id::is_valid(_id); }

	void SetFullscreen(bool isFullscreen) const;
	bool IsFullscreen() const;
	void* Handle() const;
	void SetCaption(const wchar_t* caption) const;
	void Resize(u32 width, u32 height) const;
	math::u32v4 Size() const;
	u32 Width() const;
	u32 Height() const;
	bool IsClosed() const;

private:
	window_id _id{ id::INVALID_ID };
};

}