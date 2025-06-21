#pragma once
#include "CommonHeaders.h"
#include <Windows.h>

namespace mofu::input {
struct InputState;

class InputBackend
{
public:
	void Update(InputState& state);
};

HRESULT ProcessInputMessage(HWND hwnd, u32 msg, WPARAM wParam, LPARAM lParam);
}