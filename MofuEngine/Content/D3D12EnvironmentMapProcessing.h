#pragma once
#include "CommonHeaders.h"
#include <DirectXTex.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dxgi1_6.h>

namespace mofu::content::texture {
HRESULT EquirectangularToCubemap(const DirectX::Image* envMaps, u32 envMapCount, u32 cubemapSize,
	bool usePrefilterSize, bool mirrorCubemap, DirectX::ScratchImage& cubeMaps);

HRESULT PrefilterDiffuse(const DirectX::ScratchImage& cubemaps, u32 sampleCount, DirectX::ScratchImage& prefilteredDiffuse);
HRESULT PrefilterSpecular(const DirectX::ScratchImage& cubemaps, u32 sampleCount, DirectX::ScratchImage& prefilteredSpecular);
HRESULT BrdfIntegrationLut(u32 sampleCount, DirectX::ScratchImage& brdfLut);
}