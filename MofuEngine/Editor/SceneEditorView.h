#pragma once
#include "CommonHeaders.h"
#include "ECS/Entity.h"
#include <filesystem>

namespace mofu::editor
{
void AddEntityToSceneView(ecs::Entity entity);

bool InitializeSceneEditorView();
void ShutdownSceneEditorView();
void RenderSceneEditorView();
void LoadScene(const std::filesystem::path& path); //FIXME: shouldnt be there
void AddPrefab(const std::filesystem::path& path); // FIXME: shouldnt be there
}