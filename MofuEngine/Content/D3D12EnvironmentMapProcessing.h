#pragma once
#include "CommonHeaders.h"
#include <DirectXTex.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dxgi1_6.h>

namespace mofu::content::texture {
bool InitializeEnvironmentProcessing();
void ShutdownEnvironmentProcessing();

HRESULT EquirectangularToCubemapD3D12(const DirectX::Image* envMaps, u32 envMapCount, u32 cubemapSize,
	bool usePrefilterSize, bool isSpecular, bool mirrorCubemap, DirectX::ScratchImage& cubeMaps);

HRESULT PrefilterDiffuseD3D12(const DirectX::ScratchImage& cubemaps, u32 sampleCount, DirectX::ScratchImage& prefilteredDiffuse);
HRESULT PrefilterSpecularD3D12(const DirectX::ScratchImage& cubemaps, u32 sampleCount, DirectX::ScratchImage& prefilteredSpecular);
HRESULT BrdfIntegrationLutD3D12(u32 sampleCount, DirectX::ScratchImage& brdfLut);
}