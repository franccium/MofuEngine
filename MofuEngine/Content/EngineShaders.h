#pragma once
#include "CommonHeaders.h"
#include "Shaders/ShaderData.h"

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
        RayTracingLib,
        Count
    };
};

struct EngineDebugShader
{
    enum ID : u32
    {
        PostProcessPS,
        Count
    };
};
}

namespace mofu::shaders {

struct EngineShaderInfo
{
    EngineShader::ID ID;
    ShaderFileInfo Info;
};

constexpr EngineShaderInfo ENGINE_SHADER_FILES[] {
    {EngineShader::FullscreenTriangleVS, {"FullscreenTriangle.hlsl", "FullscreenTriangleVS", ShaderType::Vertex}},
    //{EngineShader::ColorFillPS, {"ColorFill.hlsl", "ColorFillPS", ShaderType::Pixel}},
    {EngineShader::PostProcessPS, {"PostProcess.hlsl", "PostProcessPS", ShaderType::Pixel}},
    {EngineShader::CalculateGridFrustumsCS, {"CalculateGridFrustums.hlsl", "CalculateGridFrustumsCS", ShaderType::Compute}},
    {EngineShader::LightCullingCS, {"LightCulling.hlsl", "LightCullingCS", ShaderType::Compute}},
    {EngineShader::RayTracingLib, {"RayTracing.hlsl", nullptr, ShaderType::Library}},
};
static_assert(_countof(ENGINE_SHADER_FILES) == EngineShader::Count);

constexpr EngineShaderInfo ENGINE_DEBUG_SHADER_FILES[]{
    {EngineShader::PostProcessPS, {"PostProcess.hlsl", "PostProcessPS", ShaderType::Pixel}},
};
static_assert(_countof(ENGINE_DEBUG_SHADER_FILES) == EngineDebugShader::Count);


namespace physics {
struct DebugShaders
{
    enum ID : u8
    {
        LineVS,
        LinePS,
        TriangleVS,
        TrianglePS,
        FontVS,
        FontPS,
        Count
    };
};

constexpr const char* SHADERS_SRC_PATH{ "../MofuEngine/Physics/DebugRenderer/Shaders/D3D12/" };
constexpr const char* SHADERS_BIN_PATHS[DebugShaders::Count]{
    ".\\shaders\\d3d12\\physics\\PhysicsShaders.bin",
};

struct PhysicsShaderInfo
{
    DebugShaders::ID ID;
    ShaderFileInfo Info;
};

constexpr PhysicsShaderInfo SHADER_FILES[]{
    {DebugShaders::LineVS, {"Line.hlsl", "LineVS", ShaderType::Vertex}},
    {DebugShaders::LinePS, {"Line.hlsl", "LinePS", ShaderType::Pixel}},
    {DebugShaders::TriangleVS, {"Triangle.hlsl", "TriangleVS", ShaderType::Vertex}},
    {DebugShaders::TrianglePS, {"Triangle.hlsl", "TrianglePS", ShaderType::Pixel}},
    {DebugShaders::FontVS, {"Font.hlsl", "FontVS", ShaderType::Vertex}},
    {DebugShaders::FontPS, {"Font.hlsl", "FontPS", ShaderType::Pixel}}
};
static_assert(_countof(SHADER_FILES) == DebugShaders::Count);

}


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
    ShaderFileInfo Info;
};

constexpr ContentShaderInfo CONTENT_SHADER_FILES[] {
    /*{ContentShader::EnvMapProcessing_EquirectangularToCubeMapCS, {"EnvMapProcessingCS.hlsl", "EquirectangularToCubeMapCS", ShaderType::Compute}},
    {ContentShader::EnvMapProcessing_PrefilterDiffuseEnvMapCS, {"EnvMapProcessingCS.hlsl", "PrefilterDiffuseEnvMapCS", ShaderType::Compute}},
    {ContentShader::EnvMapProcessing_PrefilterSpecularEnvMapCS, {"EnvMapProcessingCS.hlsl", "PrefilterSpecularEnvMapCS", ShaderType::Compute}},
    {ContentShader::EnvMapProcessing_ComputeBRDFIntegrationLUTCS, {"EnvMapProcessingCS.hlsl", "ComputeBRDFIntegrationLUTCS", ShaderType::Compute}},*/
    {ContentShader::EnvMapProcessing_EquirectangularToCubeMapCS, {"EnvironmentMapProcessingCS.hlsl", "EquirectangularToCubeMapCS", ShaderType::Compute}},
    {ContentShader::EnvMapProcessing_PrefilterDiffuseEnvMapCS, {"EnvironmentMapProcessingCS.hlsl", "PrefilterDiffuseEnvMapCS", ShaderType::Compute}},
    {ContentShader::EnvMapProcessing_PrefilterSpecularEnvMapCS, {"EnvironmentMapProcessingCS.hlsl", "PrefilterSpecularEnvMapCS", ShaderType::Compute}},
    {ContentShader::EnvMapProcessing_ComputeBRDFIntegrationLUTCS, {"EnvironmentMapProcessingCS.hlsl", "ComputeBRDFIntegrationLUTCS", ShaderType::Compute}},
};

static_assert(_countof(CONTENT_SHADER_FILES) == ContentShader::Count);

constexpr const char* CONTENT_SHADERS_COMPILED_PATH{ "EditorAssets/Shaders/ContentShaders.bin" };
constexpr const char* CONTENT_SHADERS_SOURCE_PATH{ "../MofuEngine/Content/Shaders/" };

}
}

