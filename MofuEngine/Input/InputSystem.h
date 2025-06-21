#pragma once
#include "Win32Input.h"
#include "InputState.h"

namespace mofu::input {

[[nodiscard]] bool IsKeyDown(Keys::Key key);
[[nodiscard]] bool WasKeyPressed(Keys::Key key);
[[nodiscard]] bool WasKeyReleased(Keys::Key key);
[[nodiscard]] v2 GetMousePosition();
[[nodiscard]] v2 GetMouseDelta();
[[nodiscard]] f32 GetMouseWheelDelta();

void Update();

}