#pragma once
#include "CommonHeaders.h"
#include "ECS/Component.h"
#include "ECS/Transform.h"
#include "Graphics/Renderer.h"

namespace mofu::editor::material {
struct EditorMaterial
{
	std::string Name;
	id_t* TextureIDs;
	graphics::MaterialSurface Surface;
	graphics::MaterialType::type Type;
	u32 TextureCount;
	id_t ShaderIDs[graphics::ShaderType::Count]{ id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID };

	enum Flags : u32
	{
		None = 0x0,
		TextureRepeat = 0x01,
	};
	u32 Flags;
};

void DisplayMaterialSurfaceProperties(const graphics::MaterialSurface& mat);

bool InitializeMaterialEditor();
void RenderMaterialEditor();

void OpenMaterialEditor(ecs::Entity entityID, ecs::component::RenderMaterial mat);
void OpenMaterialCreator(ecs::Entity entityID);
}