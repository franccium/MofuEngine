#pragma once
#include "CommonHeaders.h"

namespace DirectX {
struct Image;
}

namespace mofu::content::texture {
bool IsNormalMap(const DirectX::Image* const image);
}