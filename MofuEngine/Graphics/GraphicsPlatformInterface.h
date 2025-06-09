#pragma once
#include "CommonHeaders.h"
#include "Renderer.h"
#include "Platform/Window.h"
#include <functional>
#include "EngineAPI/Camera.h"
#include "ECS/Entity.h"

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

	struct
	{
		Camera(*create)(CameraInitInfo);
		void(*remove)(camera_id);
		void(*setProperty)(camera_id, CameraProperty::Property, const void* const, u32);
		void(*getProperty)(camera_id, CameraProperty::Property, void* const, u32);
	} camera;

	struct
	{
		id_t(*addSubmesh)(const u8*&);
		void(*removeSubmesh)(id_t);

		id_t(*addTexture)(const u8* const);
		void(*removeTexture)(id_t);

		id_t(*addMaterial)(const MaterialInitInfo&);
		void(*removeMaterial)(id_t);

		id_t(*addRenderItem)(ecs::Entity, id_t, u32, const id_t* const);
		void(*removeRenderItem)(id_t);
	} resources;

	GraphicsPlatform platform = (GraphicsPlatform) - 1;
};

}