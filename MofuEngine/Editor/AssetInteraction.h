#pragma once
#include "CommonHeaders.h"
#include <filesystem>

namespace mofu::editor
{
void DropModelIntoScene(std::filesystem::path modelPath, u32* materials = nullptr);
}