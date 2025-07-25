#pragma once
#include "CommonHeaders.h"
#include <DirectXTex.h>
#include <dxgi1_6.h>

namespace mofu::content::texture {
HRESULT EquirectangularToCubemap(const DirectX::Image* envMaps, u32 envMapCount, u32 cubemapSize,
	bool usePrefilterSize, bool isSpecular, bool mirrorCubemap, DirectX::ScratchImage& cubeMaps, ID3D11Device* const device);
HRESULT EquirectangularToCubemap(const DirectX::Image* envMaps, u32 envMapCount, u32 cubemapSize,
	bool usePrefilterSize, bool isSpecular, bool mirrorCubemap, DirectX::ScratchImage& cubeMaps);

HRESULT PrefilterDiffuse(const DirectX::ScratchImage& cubemaps, u32 sampleCount, DirectX::ScratchImage& prefilteredDiffuse, ID3D11Device* const device);
HRESULT PrefilterSpecular(const DirectX::ScratchImage& cubemaps, u32 sampleCount, DirectX::ScratchImage& prefilteredSpecular, ID3D11Device* const device);
HRESULT BrdfIntegrationLut(u32 sampleCount, DirectX::ScratchImage& brdfLut, ID3D11Device* const device);
}