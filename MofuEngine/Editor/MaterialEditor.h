#pragma once
#include "CommonHeaders.h"
#include "ECS/Component.h"
#include "ECS/Transform.h"
#include "Graphics/Renderer.h"
#include "Material.h"

namespace mofu::editor::material {

void DisplayMaterialSurfaceProperties(const graphics::MaterialSurface& mat);
void DisplayEditableMaterialSurfaceProperties(graphics::MaterialSurface& mat);

bool InitializeMaterialEditor();
void RenderMaterialEditor();

void OpenMaterialEditor(ecs::Entity entityID);
void OpenMaterialView(content::AssetHandle handle);
void OpenMaterialCreator(ecs::Entity entityID);

id_t GetDefaultTextureID(TextureUsage::Usage usage);
graphics::MaterialInitInfo GetDefaultMaterialInitInfo();
EditorMaterial GetDefaultEditorMaterial();
EditorMaterial GetDefaultEditorMaterialUntextured();
void RefreshMaterials();

bool IsInvalidNormalMap(id_t texId);
}