#if defined(__INTELLISENSE__)
#define INCLUDE_WIN_PLATFORM_CODE
#endif

#ifdef INCLUDE_WIN_PLATFORM_CODE
#ifndef WIN_PLATFORM_CODE_INCLUDED
#define WIN_PLATFORM_CODE_INCLUDED
#include "Platform.h"

#include "Input/Win32Input.h"

namespace mofu::platform {
namespace {
struct WindowInfo
{
    HWND hwnd{ nullptr };
    RECT clientArea{ 0, 0, 1920, 1080 };
    RECT fullscreenArea{ 0, 0 };
    POINT topLeft{ 0, 0 };
    DWORD style{ WS_VISIBLE };
    bool isFullscreen{ false };
    bool isClosed{ false };
};

bool wasResized{ false };
util::FreeList<WindowInfo> windows;

WindowInfo&
GetFromID(window_id id)
{
    assert(windows[id].hwnd);
    return windows[id];
}

WindowInfo&
GetFromHandle(WindowHandle handle)
{
    const window_id id{ static_cast<window_id>(GetWindowLongPtr(handle, GWLP_USERDATA)) };
    return GetFromID(id);
}

LRESULT CALLBACK
InternalWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_NCCREATE:
    {
        DEBUG_OP(SetLastError(0));
        window_id id{ windows.add() };
        windows[id].hwnd = hwnd;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)id);
        assert(GetLastError() == 0);
        break;
    }
    case WM_DESTROY:
        GetFromHandle(hwnd).isClosed = true;
        break;
    case WM_SIZE:
        wasResized = (wParam != SIZE_MINIMIZED);
        break;
    default:
        break;
    }

    input::ProcessInputMessage(hwnd, msg, wParam, lParam);

    if (wasResized && GetKeyState(VK_LBUTTON) >= 0)
    {
        WindowInfo& info{ GetFromHandle(hwnd) };
        GetClientRect(info.hwnd, info.isFullscreen ? &info.fullscreenArea : &info.clientArea);
        wasResized = false;
    }

    if (msg == WM_SYSCOMMAND && wParam == SC_KEYMENU)
    {
        // blocks stuff like alt f10
        return 0;
    }

    LONG_PTR long_ptr{ GetWindowLongPtr(hwnd, 0) };
    return long_ptr ? ((WindowEventHandler)long_ptr)(hwnd, msg, wParam, lParam) : DefWindowProc(hwnd, msg, wParam, lParam);
}

void ResizeWindow(const WindowInfo& info, RECT area)
{
    AdjustWindowRect(&area, info.style, FALSE);
    const s32 width{ area.right - area.left };
    const s32 height{ area.bottom - area.top };
    MoveWindow(info.hwnd, info.topLeft.x, info.topLeft.y, width, height, true);
}

void ResizeWindow(window_id id, u32 width, u32 height)
{
    WindowInfo& info{ GetFromID(id) };
    if (info.style & WS_CHILD)
    {
        GetClientRect(info.hwnd, &info.clientArea);
    }
    else
    {
        RECT area{ info.isFullscreen ? info.fullscreenArea : info.clientArea };
        area.right = area.left + width;
        area.bottom = area.top + height;
        ResizeWindow(info, area);
    }
}

void
SetWindowFullscreen(window_id id, bool isFullscreen)
{
    WindowInfo& info{ GetFromID(id) };
    if (info.isFullscreen != isFullscreen)
    {
        info.isFullscreen = isFullscreen;
        if (isFullscreen)
        {
            // store the current window dimensions for future restoration
            GetClientRect(info.hwnd, &info.clientArea);
            RECT rect;
            GetWindowRect(info.hwnd, &rect);
            info.topLeft.x = rect.left;
            info.topLeft.y = rect.top;
            SetWindowLongPtr(info.hwnd, GWL_STYLE, 0);
            ShowWindow(info.hwnd, SW_MAXIMIZE);
        }
        else
        {
            SetWindowLongPtr(info.hwnd, GWL_STYLE, info.style);
            ResizeWindow(info, info.clientArea);
            ShowWindow(info.hwnd, SW_SHOWNORMAL);
        }
    }
}

WindowHandle
GetWindowHandle(window_id id)
{
    return GetFromID(id).hwnd;
}

bool
IsWindowClosed(window_id id)
{
    return GetFromID(id).isClosed;
}

bool
IsWindowFullscreen(window_id id)
{
    return GetFromID(id).isFullscreen;
}

u32v4
GetWindowSize(window_id id)
{
    WindowInfo& info{ GetFromID(id) };
    RECT& area{ info.isFullscreen ? info.fullscreenArea : info.clientArea };
    return { (u32)area.left, (u32)area.top, (u32)area.right, (u32)area.bottom };
}

void
SetWindowCaption(window_id id, const wchar_t* caption)
{
    WindowInfo& info{ GetFromID(id) };
    SetWindowText(info.hwnd, caption);
}

} // anonymous namespace

Window
ConcoctWindow(const WindowInitInfo* const initInfo)
{
    WindowEventHandler callback{ initInfo ? initInfo->windowEventCallback : nullptr };
    WindowHandle parent{ initInfo ? initInfo->parentWindow : nullptr };

    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = InternalWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = callback ? sizeof(callback) : 0;
    wc.hInstance = 0;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDI_APPLICATION);
    wc.hbrBackground = CreateSolidBrush(RGB(26, 48, 76));
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"MofuWindow";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassEx(&wc);

    WindowInfo info{};
    info.clientArea.right = initInfo && initInfo->width ? info.clientArea.left + initInfo->width : info.clientArea.right;
    info.clientArea.bottom = initInfo && initInfo->height ? info.clientArea.top + initInfo->height : info.clientArea.bottom;
    info.style |= parent ? WS_CHILD : WS_OVERLAPPEDWINDOW;
    RECT rc{ info.clientArea };
    AdjustWindowRect(&rc, info.style, FALSE);

    const wchar_t* caption{ (initInfo && initInfo->caption ? initInfo->caption : L"Game") };
    const s32 left{ initInfo ? initInfo->left : info.topLeft.x };
    const s32 top{ initInfo ? initInfo->top : info.topLeft.y };
    const s32 width{ rc.right - rc.left };
    const s32 height{ rc.bottom - rc.top };

    info.hwnd = CreateWindowEx(
        0, wc.lpszClassName, caption, info.style, left, top, width, height, parent, NULL, NULL, NULL
    );

    if (info.hwnd)
    {
        DEBUG_OP(SetLastError(0));
        if (callback) SetWindowLongPtr(info.hwnd, 0, (LONG_PTR)callback);
        assert(GetLastError() == 0);
        ShowWindow(info.hwnd, SW_SHOWNORMAL);
        UpdateWindow(info.hwnd);
        window_id id{ (id_t)GetWindowLongPtr(info.hwnd, GWLP_USERDATA) };
        windows[id] = info;
        return Window{ id };
    }

    return {};
}

void
RemoveWindow(window_id id)
{
    WindowInfo& info{ GetFromID(id) };
    DestroyWindow(info.hwnd);
    windows.remove(id);
}

}
#endif
#endif