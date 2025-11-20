#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::resources {
struct ResourceUpdateState
{
	enum ResourceID : u8
	{
		GPassBuffers = (1u << 0)
	};

	u8 UpdatedResources{ 0 };
};

void SetResourceUpdated(ResourceUpdateState::ResourceID id);
bool WasResourceUpdated(ResourceUpdateState::ResourceID id);
void ClearWatch();

}