#include "Platform/Platform.h"
using namespace mofu;

namespace {


LRESULT 
WindowEventHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	default:
		break;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}
}

bool MofuInitialize()
{
	// load the game content
	// create the game window
	return true;
}

void MofuUpdate()
{
	// update the game
}

void MofuShutdown()
{
	// destroy the window
	// unload game content
}