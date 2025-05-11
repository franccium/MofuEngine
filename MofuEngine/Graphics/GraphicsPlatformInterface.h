#pragma once
#include "CommonHeaders.h"
#include "Renderer.h"
#include "Platform/Window.h"

namespace mofu::graphics {
struct PlatformInterface
{
	bool(*initialize)(void);
	void(*shutdown)(void);

	struct
	{
		Surface(*create)(platform::Window);
		void(*remove)(surface_id);
		void(*render)(surface_id, FrameInfo);
		void(*resize)(surface_id, u32 width, u32 height);
		u32(*width)(surface_id);
		u32(*height)(surface_id);
	} surface;

	GraphicsPlatform platform = (GraphicsPlatform) - 1;
};

}