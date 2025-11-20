#pragma once
#include "D3D12ResourceWatch.h"

namespace mofu::graphics::d3d12::resources {
namespace {
ResourceUpdateState _updateState{};
}

void SetResourceUpdated(ResourceUpdateState::ResourceID id)
{
	_updateState.UpdatedResources |= id;
}

bool WasResourceUpdated(ResourceUpdateState::ResourceID id)
{
	return _updateState.UpdatedResources & id;
}

void ClearWatch()
{
	_updateState.UpdatedResources = 0;
}

}