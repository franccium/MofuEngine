#pragma once
#include "CommonHeaders.h"
#include "ECS/Entity.h"

namespace mofu::editor
{
void AddEntityToSceneView(ecs::Entity entity);

bool InitializeSceneEditorView();
void RenderSceneEditorView();

}