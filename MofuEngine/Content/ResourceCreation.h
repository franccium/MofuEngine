#pragma once
#include "CommonHeaders.h"
#include "ContentManagement.h"

namespace mofu::content {


struct LodOffset
{
	u16 Offset;
	u16 Count;
};

id_t CreateResourceFromBlob(const void* const blob, AssetType::type resourceType);
void DestroyResource(id_t resourceId, AssetType::type resourceType);
}