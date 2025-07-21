#pragma once
#include "CommonHeaders.h"
#include <DirectXTex.h>
#include <dxgi1_6.h>

namespace mofu::content::texture {
HRESULT EquirectangularToCubemap(const DirectX::Image* envMaps, u32 envMapCount, u32 cubemapSize,
	bool usePrefilterSize, bool mirrorCubemap, DirectX::ScratchImage& cubeMaps);
HRESULT EquirectangularToCubemap(const ID3D11Device* device, const DirectX::Image* envMaps, u32 envMapCount, u32 cubemapSize,
	bool usePrefilterSize, bool mirrorCubemap, DirectX::ScratchImage& cubeMaps);
HRESULT PrefilterDiffuse(const ID3D11Device* device, const DirectX::ScratchImage& cubemaps, u32 sampleCount, DirectX::ScratchImage& prefilteredDiffuse);
HRESULT PrefilterSpecular(const ID3D11Device* device, const DirectX::ScratchImage& cubemaps, u32 sampleCount, DirectX::ScratchImage& prefilteredSpecular);
HRESULT BrdfIntegrationLut(const ID3D11Device* device, u32 sampleCount, DirectX::ScratchImage& brdfLut);
}