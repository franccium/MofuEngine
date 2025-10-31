#include "InputSystem.h"

namespace mofu::input {
namespace {
InputState _inputState{};
InputBackend _inputBackend;

v2 mouseSensitivity{ 0.01f, 0.01f };
}

bool
IsKeyDown(Keys::Key key)
{
    return _inputState.KeyDown[key];
}

bool 
WasKeyPressed(Keys::Key key)
{
    return _inputState.KeyPressed[key];
}

bool
WasKeyPressed(Keys::Key key, Keys::Key modifier)
{
    return _inputState.KeyDown[modifier] && _inputState.KeyPressed[key];
}

bool WasKeyPressed(ModdedKey keyMod)
{
    return _inputState.KeyDown[keyMod.Mod] && _inputState.KeyPressed[keyMod.Key];
}

bool
WasKeyReleased(Keys::Key key)
{
    return _inputState.KeyReleased[key];
}

v2 GetMousePosition()
{
    return _inputState.MousePosition;
}

v2 GetMouseDelta()
{
    v2 delta{ _inputState.MouseDelta };
    return { delta.x * mouseSensitivity.x, delta.y * mouseSensitivity.y };
}

f32 GetMouseWheelDelta()
{
    return _inputState.MouseWheelDelta;
}

void Update()
{
    _inputBackend.Update(_inputState);
}

}