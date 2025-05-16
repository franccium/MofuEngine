#pragma once
#include "CommonHeaders.h"
#include "Utilities/Logger.h"

namespace mofu {
bool InitializeEngineModules()
{
	mofu::log::Initialize();
	return true;
}
}