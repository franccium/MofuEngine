#include "EnvironmentMapProcessing.h"

using namespace DirectX;

namespace mofu::content::texture {
HRESULT
EquirectangularToCubemap(const Image* envMaps, u32 envMapCount, u32 cubemapSize, 
	bool usePrefilterSize, bool mirrorCubemap, ScratchImage& cubeMaps)
{
	return E_NOTIMPL;
}

HRESULT
EquirectangularToCubemap(const ID3D11Device* device, const Image* envMaps, u32 envMapCount, 
	u32 cubemapSize, bool usePrefilterSize, bool mirrorCubemap, ScratchImage& cubeMaps)
{
	return E_NOTIMPL;
}

HRESULT
PrefilterDiffuse(const ID3D11Device* device, const ScratchImage& cubemaps, u32 sampleCount, ScratchImage& prefilteredDiffuse)
{
	return E_NOTIMPL;
}

HRESULT
PrefilterSpecular(const ID3D11Device* device, const ScratchImage& cubemaps, u32 sampleCount, ScratchImage& prefilteredSpecular)
{
	return E_NOTIMPL;
}

HRESULT
BrdfIntegrationLut(const ID3D11Device* device, u32 sampleCount, ScratchImage& brdfLut)
{
	return E_NOTIMPL;
}

}