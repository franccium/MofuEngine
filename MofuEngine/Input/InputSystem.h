#pragma once
#include "Win32Input.h"
#include "InputState.h"

namespace mofu::input {
[[nodiscard]] bool IsKeyDown(Keys::Key key);
[[nodiscard]] bool WasKeyPressed(Keys::Key key);
[[nodiscard]] bool WasKeyPressed(Keys::Key key, Keys::Key modifier);
[[nodiscard]] bool WasKeyPressed(ModdedKey keyMod);
[[nodiscard]] bool WasKeyReleased(Keys::Key key);
[[nodiscard]] v2 GetMousePosition();
[[nodiscard]] v2 GetMouseDelta();
[[nodiscard]] f32 GetMouseWheelDelta();

void Update();

static struct Keybinds
{
	static struct
	{
		static constexpr Keys::Key Forward{ Keys::Key::W };
		static constexpr Keys::Key Backwards{ Keys::Key::S };
		static constexpr Keys::Key Left{ Keys::Key::A };
		static constexpr Keys::Key Right{ Keys::Key::D };
		static constexpr Keys::Key Up{ Keys::Key::Space };
		static constexpr Keys::Key Down{ Keys::Key::C };
		static constexpr Keys::Key Sprint{ Keys::Key::Shift };
		static constexpr Keys::Key LockRotation{ Keys::Key::R };
	} Movement;

	static struct
	{
		static constexpr Keys::Key ShaderReload{ Keys::Key::End };
		static constexpr Keys::Key ToggleRenderingConsole{ Keys::Key::K };
		static constexpr ModdedKey ToggleObjectAdder{ Keys::Key::A, Keys::Key::Alt };
	} Editor;

	static struct
	{
		static constexpr Keys::Key RTUpdate{ Keys::Key::E };
		static constexpr Keys::Key RTASRebuild{ Keys::Key::T };
	} Debug;
};

}