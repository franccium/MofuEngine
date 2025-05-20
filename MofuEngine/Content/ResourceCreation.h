#pragma once
#include "CommonHeaders.h"
#include "ContentManagement.h"

namespace mofu::content {
id_t CreateResourceFromBlob(const char* const blob, AssetType::type resourceType);
void DestroyResource(id_t resourceId, AssetType::type resourceType);
}