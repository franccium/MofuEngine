#pragma once
#include "CommonHeaders.h"
#include "ShaderCompilation.h"

namespace mofu {
struct EngineShader 
{
    enum ID : u32
    {
        FullscreenTriangleVS = 0,
        //ColorFillPS,
        PostProcessPS,
        CalculateGridFrustumsCS,
        LightCullingCS,
        Count
    };
};

struct EngineShaderInfo
{
    EngineShader::ID ID;
    shaders::ShaderFileInfo Info;
};

constexpr EngineShaderInfo ENGINE_SHADER_FILES[] {
    {EngineShader::FullscreenTriangleVS, {"FullscreenTriangle.hlsl", "FullscreenTriangleVS", graphics::ShaderType::Vertex}},
    //{EngineShader::ColorFillPS, {"ColorFill.hlsl", "ColorFillPS", graphics::ShaderType::Pixel}},
    {EngineShader::PostProcessPS, {"PostProcess.hlsl", "PostProcessPS", graphics::ShaderType::Pixel}},
    {EngineShader::CalculateGridFrustumsCS, {"CalculateGridFrustums.hlsl", "CalculateGridFrustumsCS", graphics::ShaderType::Compute}},
    {EngineShader::LightCullingCS, {"LightCulling.hlsl", "LightCullingCS", graphics::ShaderType::Compute}},
};
static_assert(_countof(ENGINE_SHADER_FILES) == EngineShader::Count);

namespace content {
struct ContentShader
{
    enum ID : u32
    {
        EnvMapProcessing_EquirectangularToCubeMapCS,
        EnvMapProcessing_PrefilterDiffuseEnvMapCS,
        EnvMapProcessing_PrefilterSpecularEnvMapCS,
        EnvMapProcessing_ComputeBRDFIntegrationLUTCS,

        Count,
    };
};

struct ContentShaderInfo
{
    ContentShader::ID ID;
    shaders::ShaderFileInfo Info;
};

constexpr ContentShaderInfo CONTENT_SHADER_FILES[] {
    {ContentShader::EnvMapProcessing_EquirectangularToCubeMapCS, {"EnvMapProcessingCS.hlsl", "EquirectangularToCubeMapCS", graphics::ShaderType::Compute}},
    {ContentShader::EnvMapProcessing_PrefilterDiffuseEnvMapCS, {"EnvMapProcessingCS.hlsl", "PrefilterDiffuseEnvMapCS", graphics::ShaderType::Compute}},
    {ContentShader::EnvMapProcessing_PrefilterSpecularEnvMapCS, {"EnvMapProcessingCS.hlsl", "PrefilterSpecularEnvMapCS", graphics::ShaderType::Compute}},
    {ContentShader::EnvMapProcessing_ComputeBRDFIntegrationLUTCS, {"EnvMapProcessingCS.hlsl", "ComputeBRDFIntegrationLUTCS", graphics::ShaderType::Compute}},
};

static_assert(_countof(CONTENT_SHADER_FILES) == ContentShader::Count);

constexpr const char* CONTENT_SHADERS_COMPILED_PATH{ "EditorAssets/Shaders/ContentShaders.bin" };
constexpr const char* CONTENT_SHADERS_SOURCE_PATH{ "../MofuEngine/Content/Shaders/" };

}
}

