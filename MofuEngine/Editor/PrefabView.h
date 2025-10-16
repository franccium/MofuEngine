#pragma once
#include "CommonHeaders.h"
#include <filesystem>

namespace mofu::editor::scene
{
void InspectPrefab(const std::filesystem::path& path);
void LoadPrefab(const std::filesystem::path& path);

}