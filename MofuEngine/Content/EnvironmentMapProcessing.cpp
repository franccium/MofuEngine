#include "EnvironmentMapProcessing.h"

using namespace DirectX;

namespace mofu::content::texture {
namespace {

constexpr u32 PREFILTERED_DIFFUSE_CUBEMAP_SIZE{ 32 };
constexpr u32 PREFILTERED_SPECULAR_CUBEMAP_SIZE{ 256 };
constexpr u32 ROUGHNESS_MIP_LEVELS{ 6 };
constexpr u32 BRDF_INTEGRATION_LUT_SIZE{ 256 };

} // anonymous namespace

HRESULT
EquirectangularToCubemap(const Image* envMaps, u32 envMapCount, u32 cubemapSize, 
	bool usePrefilterSize, bool mirrorCubemap, ScratchImage& cubeMaps)
{
	if (usePrefilterSize) cubemapSize = PREFILTERED_SPECULAR_CUBEMAP_SIZE;
	assert(envMaps && envMapCount);

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