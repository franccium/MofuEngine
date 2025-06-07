#pragma once
#include "CommonHeaders.h"
#include "Utilities/Logger.h"
#include "ECS/ECSCore.h"

namespace mofu {
bool InitializeEngineModules()
{
	mofu::log::Initialize();
	mofu::ecs::Initialize();
	return true;
}

void ShutdownEngineModules()
{
	mofu::ecs::Shutdown();
	mofu::log::Shutdown();
}
}