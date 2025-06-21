#include "Win32Input.h"
#include "InputState.h"

namespace mofu::input {
namespace {

u8 keyStates[Keys::Count];
v2 mousePosition{};
v2 mouseDelta{};
f32 mouseWheelDelta{};

v2
GetMousePosition(LPARAM lParam)
{
    return { (f32)((s16)(lParam & 0x0000ffff)), (f32)((s16)(lParam >> 16)) };
}

}

void
InputBackend::Update(InputState& state)
{
	for (u32 key{ 0 }; key < Keys::Count; ++key)
	{
        u8 isKeyDown{ keyStates[key] };
		u8 wasKeyDown{ state.KeyDown[key] };
		state.KeyDown[key] = isKeyDown;
		state.KeyPressed[key] = isKeyDown & !wasKeyDown;
		state.KeyReleased[key] = !isKeyDown & wasKeyDown;
        state.MouseDelta = mouseDelta;
        state.MouseWheelDelta = mouseWheelDelta;
        state.MousePosition = mousePosition;
	}
    mouseDelta = v2zero;
    mouseWheelDelta = 0.f;
}

HRESULT
ProcessInputMessage(HWND hwnd, u32 msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        Keys::Key key = (Keys::Key)wParam;
        keyStates[key] = 1;
    }
    break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        Keys::Key key = (Keys::Key)wParam;
        keyStates[key] = 0;
    }
    break;
    case WM_MOUSEMOVE:
    {
        const v2 pos{ GetMousePosition(lParam) };
        mouseDelta.x = pos.x - mousePosition.x;
        mouseDelta.y = pos.y - mousePosition.y;
        mousePosition = pos;
    }
    break;
    case WM_MOUSEWHEEL:
    {
        mouseWheelDelta = (f32)GET_WHEEL_DELTA_WPARAM(wParam);
    }
    break;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    {
        SetCapture(hwnd);
        const Keys::Key key{ msg == WM_LBUTTONDOWN ? Keys::MouseLMB : msg == WM_RBUTTONDOWN ? Keys::MouseRMB : Keys::MouseMMB };
        const v2 pos{ GetMousePosition(lParam) };
        keyStates[key] = 1;
    }
    break;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    {
        ReleaseCapture();
        const Keys::Key key{ msg == WM_LBUTTONUP ? Keys::MouseLMB : msg == WM_RBUTTONUP ? Keys::MouseRMB : Keys::MouseMMB };
        const v2 pos{ GetMousePosition(lParam) };
        keyStates[key] = 0;
    }
    break;
    }

    return S_OK;
}

}