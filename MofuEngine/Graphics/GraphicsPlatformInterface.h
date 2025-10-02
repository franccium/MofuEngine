#pragma once
#include "CommonHeaders.h"
#include "GraphicsPlatform.h"
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
		void(*getDescriptorIndices)(const id_t*const, u32, u32* const);

		id_t(*addMaterial)(const MaterialInitInfo&);
		void(*removeMaterial)(id_t);
		MaterialInitInfo(*getMaterialReflection)(const id_t);

		id_t(*addRenderItem)(ecs::Entity, id_t, u32, const id_t);
		void(*removeRenderItem)(id_t);
		void(*updateRenderItemData)(id_t, id_t);
	} resources;

	struct
	{
		void(*initialize)();
		void(*shutdown)();
		void(*startNewFrame)();
		void(*viewTexture)(id_t);
		void(*destroyViewTexture)(id_t);
		u64(*getImTextureIDIcon)(id_t, u32, u32);
		u64(*getImTextureID)(id_t, u32, u32, u32, u32, bool);
		id_t(*addIcon)(const u8* const);
	} ui;

	GraphicsPlatform platform = (GraphicsPlatform) -1;
};

}