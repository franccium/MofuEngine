#pragma once
#include "CommonHeaders.h"
#include "Utilities/Logger.h"
#include "ECS/Scene.h"

namespace mofu {
bool InitializeEngineModules()
{
	mofu::log::Initialize();
	mofu::ecs::InitializeECS();
	return true;
}
}