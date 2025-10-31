#pragma once
#include "CommonHeaders.h"
#include "ECS/Entity.h"
#include <filesystem>

namespace mofu::editor::object
{
void Initialize();
void UpdateObjectPickerProbe();
ecs::Entity GetPickedEntity();
}