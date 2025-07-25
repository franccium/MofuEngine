#include "D3D12EnvironmentMapProcessing.h"

using namespace DirectX;

namespace mofu::content::texture {
namespace {
constexpr u32 PREFILTERED_DIFFUSE_CUBEMAP_SIZE{ 32 };
constexpr u32 PREFILTERED_SPECULAR_CUBEMAP_SIZE{ 256 };
constexpr u32 ROUGHNESS_MIP_LEVELS{ 6 };
constexpr u32 BRDF_INTEGRATION_LUT_SIZE{ 256 };

struct ShaderConstants
{
	u32 CubeMapInSize;
	u32 CubeMapOutSize;
	u32 SampleCount; // used for prefiltered, but also as a sign to mirror the cubemap when its == 1
	f32 Roughness;
};




}


HRESULT EquirectangularToCubemap(const Image* envMaps, u32 envMapCount, u32 cubemapSize,
	bool usePrefilterSize, bool mirrorCubemap, ScratchImage& cubeMaps)
{
	return E_NOTIMPL;
}

HRESULT PrefilterDiffuse(const ScratchImage& cubemaps, u32 sampleCount, ScratchImage& prefilteredDiffuse)
{
	return E_NOTIMPL;
}
HRESULT PrefilterSpecular(const ScratchImage& cubemaps, u32 sampleCount, ScratchImage& prefilteredSpecular)
{
	return E_NOTIMPL;
}
HRESULT BrdfIntegrationLut(u32 sampleCount, ScratchImage& brdfLut)
{
	return E_NOTIMPL;
}
}