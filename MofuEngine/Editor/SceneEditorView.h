#pragma once
#include "CommonHeaders.h"
#include "ECS/Entity.h"
#include <filesystem>

namespace mofu::editor
{
void AddEntityToSceneView(ecs::Entity entity);

bool InitializeSceneEditorView();
void RenderSceneEditorView();
void ImportScene(const std::filesystem::path& path); //FIXME: shouldnt be there
}