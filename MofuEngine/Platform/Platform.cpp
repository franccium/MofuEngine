#include "Platform.h"

#ifdef _WIN64
#include "Win32Platform.cpp"
#elif

#endif

namespace mofu::platform {

void 
Window::SetFullscreen(bool isFullscreen) const
{
    assert(IsValid());
    SetWindowFullscreen(_id, isFullscreen);
}

bool 
Window::IsFullscreen() const
{
    assert(IsValid());
    return IsWindowFullscreen(_id);
}

void* 
Window::Handle() const
{
    assert(IsValid());
    return GetWindowHandle(_id);
}

void 
Window::SetCaption(const wchar_t* caption) const
{
    assert(IsValid());
    return SetWindowCaption(_id, caption);
}

void 
Window::Resize(u32 width, u32 height) const
{
    assert(IsValid());
    return ResizeWindow(_id, width, height);
}

u32v4 
Window::Size() const
{
    assert(IsValid());
    return GetWindowSize(_id);
}

u32 
Window::Width() const
{
    assert(IsValid());
    u32v4 size{ Size() };
    return size.z - size.x;
}

u32 
Window::Height() const
{
    assert(IsValid());
    u32v4 size{ Size() };
    return size.w - size.y;
}

bool 
Window::IsClosed() const
{
    assert(IsValid());
    return IsWindowClosed(_id);
}

}