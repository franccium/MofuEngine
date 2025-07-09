#pragma once
#include "CommonHeaders.h"
#include "ECS/Component.h"
#include "ECS/Transform.h"

namespace mofu::editor::material {
bool InitializeMaterialEditor();
void RenderMaterialEditor();

void OpenMaterialEditor(ecs::Entity entityID, ecs::component::RenderMaterial mat);
void OpenMaterialCreator(ecs::Entity entityID);
}