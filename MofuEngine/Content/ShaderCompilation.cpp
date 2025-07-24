#include "ShaderCompilation.h"
#include <filesystem>
#include <wrl.h>
#include <dxcapi.h> 
#include <d3d12shader.h>
#include <fstream>
#include "Graphics/Renderer.h"
#include "ContentManagement.h"
#include "Content/ResourceCreation.h"

using namespace mofu;
using namespace Microsoft::WRL;

#ifdef _DEBUG
#ifndef DXCall
#define DXCall(x)\
if(FAILED(x)) {\
	char line_number[32];\
	sprintf_s(line_number, "%u", __LINE__);\
	OutputDebugStringA("Error in: ");\
	OutputDebugStringA(__FILE__);\
	OutputDebugStringA("\nline: ");\
	OutputDebugStringA(line_number);\
	OutputDebugStringA("\n");\
	OutputDebugStringA(#x);\
	OutputDebugStringA("\n");\
	__debugbreak();\
}
#endif
#else
#ifndef DXCall
#define DXCall(x) x
#endif
#endif

namespace {
constexpr const char* SHADERS_SOURCE_PATH{ "../MofuEngine/Graphics/D3D12/Shaders/" };

constexpr std::wstring
c_to_wstring(const char* c)
{
    std::string str{ c };
    return{ str.begin(), str.end() };
}

struct EngineShader {
    enum id : u32
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
    EngineShader::id id;
    ShaderFileInfo info;
};

constexpr EngineShaderInfo ENGINE_SHADER_FILES[]
{
    {EngineShader::FullscreenTriangleVS, {"FullscreenTriangle.hlsl", "FullscreenTriangleVS", graphics::ShaderType::Vertex}},
    //{EngineShader::ColorFillPS, {"ColorFill.hlsl", "ColorFillPS", graphics::ShaderType::Pixel}},
    {EngineShader::PostProcessPS, {"PostProcess.hlsl", "PostProcessPS", graphics::ShaderType::Pixel}},
    {EngineShader::CalculateGridFrustumsCS, {"CalculateGridFrustums.hlsl", "CalculateGridFrustumsCS", graphics::ShaderType::Compute}},
    {EngineShader::LightCullingCS, {"LightCulling.hlsl", "LightCullingCS", graphics::ShaderType::Compute}},
};
static_assert(_countof(ENGINE_SHADER_FILES) == EngineShader::Count);

struct DxcCompiledShader
{
    ComPtr<IDxcBlob> bytecode;
    ComPtr<IDxcBlobUtf8> errors;
    ComPtr<IDxcBlobUtf8> assembly;
    DxcShaderHash hash;
};

class ShaderCompiler
{
public:
    ShaderCompiler()
    {
        HRESULT hr{ S_OK };
        DXCall(hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&_compiler)));
        if (FAILED(hr)) return;
        DXCall(hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&_utils)));
        if (FAILED(hr)) return;
        DXCall(hr = _utils->CreateDefaultIncludeHandler(&_includeHandler));
        if (FAILED(hr)) return;
    }
    DISABLE_COPY_AND_MOVE(ShaderCompiler);

    DxcCompiledShader Compile(IDxcBlobEncoding* sourceBlob, Vec<std::wstring> compilerArgs)
    {
        DxcBuffer buffer{};
        buffer.Encoding = DXC_CP_ACP;
        buffer.Ptr = sourceBlob->GetBufferPointer();
        buffer.Size = sourceBlob->GetBufferSize();

        Vec<LPCWSTR> args{};
        for (const auto& arg : compilerArgs)
            args.emplace_back(arg.c_str());

        HRESULT hr{ S_OK };
        ComPtr<IDxcResult> results{ nullptr };
        DXCall(hr = _compiler->Compile(&buffer, args.data(), (u32)args.size(), _includeHandler.Get(), IID_PPV_ARGS(&results)));
        if (FAILED(hr)) return {};

        ComPtr<IDxcBlobUtf8> errors{ nullptr };
        DXCall(hr = results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));

        if (errors && errors->GetBufferSize())
        {
            OutputDebugStringA("\n Shader Compilation Error: \n");
            OutputDebugStringA(errors->GetStringPointer());
        }
        else
        {
            DEBUG_LOG("[Compilation Succeded]");
        }
        OutputDebugStringA("\n");

        HRESULT status{ S_OK };
        DXCall(hr = results->GetStatus(&status));
        if (FAILED(hr) || FAILED(status)) return {};

        ComPtr<IDxcBlob> hash{ nullptr };
        DXCall(hr = results->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&hash), nullptr));
        if (FAILED(hr)) return {};
        DxcShaderHash* const hashBuffer{ (DxcShaderHash* const)hash->GetBufferPointer() };
        assert(!(hashBuffer->Flags & DXC_HASHFLAG_INCLUDES_SOURCE));
        OutputDebugStringA("Shader hash: ");
        for (u32 i{ 0 }; i < _countof(hashBuffer->HashDigest); ++i)
        {
            char hashBytes[3]{};
            sprintf_s(hashBytes, "%02x", (u32)hashBuffer->HashDigest[i]);
            OutputDebugStringA(hashBytes);
            OutputDebugStringA(" ");
        }
        OutputDebugStringA("\n");

        ComPtr<IDxcBlob> shader{ nullptr };
        DXCall(hr = results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader), nullptr));
        if (FAILED(hr)) return {};

        buffer.Ptr = shader->GetBufferPointer();
        buffer.Size = shader->GetBufferSize();
        ComPtr<IDxcResult> disassemblyResults{ nullptr };
        ComPtr<IDxcBlobUtf8> disassembly{ nullptr };
        DXCall(hr = _compiler->Disassemble(&buffer, IID_PPV_ARGS(&disassemblyResults)));
        DXCall(hr = disassemblyResults->GetOutput(DXC_OUT_DISASSEMBLY, IID_PPV_ARGS(&disassembly), nullptr));

        DxcCompiledShader result{ shader.Detach(), errors.Detach(), disassembly.Detach() };
        memcpy(&result.hash, &hashBuffer->HashDigest[0], _countof(hashBuffer->HashDigest));

        char shaderSizeInfo[32];
        sprintf_s(shaderSizeInfo, sizeof(shaderSizeInfo), "Shader size: %zu\n", buffer.Size);
        DEBUG_LOG(shaderSizeInfo);

        return result;
    }

    DxcCompiledShader Compile(u8* data, u32 dataSize, graphics::ShaderType::type type,
        const char* entryPoint, Vec<std::wstring>& extraArgs)
    {
        assert(data && dataSize && entryPoint);
        assert(_compiler && _utils && _includeHandler);
        assert(type < graphics::ShaderType::Count);

        HRESULT hr{ S_OK };
        ComPtr<IDxcBlobEncoding> sourceBlob{ nullptr };
        DXCall(hr = _utils->CreateBlob(data, dataSize, 0, &sourceBlob));
        if (FAILED(hr)) return {};
        assert(sourceBlob && sourceBlob->GetBufferSize());

        ShaderFileInfo info{};
        info.entryPoint = entryPoint;
        info.type = type;

        OutputDebugStringA("Compiling ");
        OutputDebugStringA(info.entryPoint);
        OutputDebugStringA("\n");

        return Compile(sourceBlob.Get(), GetCompilerArgs(info, extraArgs));
    }

    DxcCompiledShader Compile(ShaderFileInfo info, std::filesystem::path fullPath, Vec<std::wstring>& extraArgs)
    {
        assert(_compiler && _utils && _includeHandler);
        HRESULT hr{ S_OK };

        ComPtr<IDxcBlobEncoding> sourceBlob{ nullptr };
        DXCall(hr = _utils->LoadFile(fullPath.c_str(), nullptr, &sourceBlob));
        if (FAILED(hr)) return {};
        assert(sourceBlob && sourceBlob->GetBufferSize());

        OutputDebugStringA("Compiling ");
        OutputDebugStringA(info.file);
        OutputDebugStringA(" : ");
        OutputDebugStringA(info.entryPoint);
        OutputDebugStringA("\n");

        return Compile(sourceBlob.Get(), GetCompilerArgs(info, extraArgs));
    }

private:
    constexpr static u8 DEFAULT_COMPILER_ARGS_COUNT{ 13 };
    Vec<std::wstring> GetCompilerArgs(const ShaderFileInfo& info, Vec<std::wstring>& extraArgs)
    {
        Vec<std::wstring> args((u64)DEFAULT_COMPILER_ARGS_COUNT + extraArgs.size());
        if (info.file) args.emplace_back(c_to_wstring(info.file));
        args.emplace_back(L"-E");
        args.emplace_back(c_to_wstring(info.entryPoint));
        args.emplace_back(L"-T");
        args.emplace_back(c_to_wstring(SHADER_PROFILES[(u32)info.type]));
        args.emplace_back(L"-I");
        args.emplace_back(c_to_wstring(SHADERS_SOURCE_PATH));
        args.emplace_back(L"-enable-16bit-types");
        args.emplace_back(DXC_ARG_ALL_RESOURCES_BOUND);
#if _DEBUG
        args.emplace_back(DXC_ARG_DEBUG);
        args.emplace_back(DXC_ARG_SKIP_OPTIMIZATIONS);
#else
        args.emplace_back(DXC_ARG_OPTIMIZATION_LEVEL3);
#endif
        args.emplace_back(DXC_ARG_WARNINGS_ARE_ERRORS);
        args.emplace_back(L"-Qstrip_reflect");
        args.emplace_back(L"-Qstrip_debug");

        for (const auto& arg : extraArgs)
        {
            args.emplace_back(arg);
        }
        return args;
    }

	constexpr static const char* SHADER_PROFILES[]{ "vs_6_6", "ps_6_6", "ds_6_6", "hs_6_6", "gs_6_6", "cs_6_6", "as_6_6", "ms_6_6" };
	static_assert(_countof(SHADER_PROFILES) == graphics::ShaderType::Count);

	ComPtr<IDxcCompiler3> _compiler{ nullptr };
	ComPtr<IDxcUtils> _utils{ nullptr };
	ComPtr<IDxcIncludeHandler> _includeHandler{ nullptr };
};

std::unique_ptr<u8[]>
PackShader(DxcCompiledShader shader, bool includeErrorsAndDissasembly)
{
    if (shader.bytecode && shader.bytecode->GetBufferPointer() && shader.bytecode->GetBufferSize())
    {
        static_assert(content::CompiledShader::HASH_LENGTH == _countof(DxcShaderHash::HashDigest));
        const u64 extraSize{ includeErrorsAndDissasembly
            ? sizeof(u64) + sizeof(u64) + shader.errors->GetStringLength() + shader.assembly->GetStringLength()
            : 0 };
        const u64 bufferSize{ sizeof(u64) + content::CompiledShader::HASH_LENGTH + shader.bytecode->GetBufferSize() + extraSize };

        std::unique_ptr<u8[]> buffer{ std::make_unique<u8[]>(bufferSize) };
        util::BlobStreamWriter blob{ buffer.get(), bufferSize };

        blob.Write(shader.bytecode->GetBufferSize());
        blob.WriteBytes(shader.hash.HashDigest, content::CompiledShader::HASH_LENGTH);
        blob.WriteBytes((u8*)shader.bytecode->GetBufferPointer(), shader.bytecode->GetBufferSize());

        if (includeErrorsAndDissasembly)
        {
            assert(shader.errors->GetStringPointer() && shader.assembly->GetStringPointer());
            blob.Write(shader.errors->GetStringLength());
            blob.Write(shader.assembly->GetStringLength());
            blob.WriteChars(shader.errors->GetStringPointer(), shader.errors->GetStringLength());
            blob.WriteChars(shader.assembly->GetStringPointer(), shader.assembly->GetStringLength());
        }

        assert(blob.Offset() == bufferSize);
        return buffer;
    }

    return {};
}

const std::filesystem::path 
GetEngineShadersPath()
{
    return graphics::GetEngineShadersPath(graphics::GraphicsPlatform::Direct3D12);
}

bool
SaveCompiledShaders(Vec<DxcCompiledShader>& shaders)
{
    const auto engineShadersPath{ GetEngineShadersPath() };
    std::filesystem::create_directories(engineShadersPath.parent_path());
    std::ofstream file(engineShadersPath, std::ios::out | std::ios::binary);
    if (!std::filesystem::exists(engineShadersPath) || !file || !file.is_open())
    {
        return false;
    }

    for (const auto& shader : shaders)
    {
        void* const bytecode{ shader.bytecode->GetBufferPointer() };
        const u64 bytecodeLength{ shader.bytecode->GetBufferSize() };
        file.write(reinterpret_cast<const char*>(&bytecodeLength), sizeof(bytecodeLength));
        file.write(reinterpret_cast<const char*>(shader.hash.HashDigest), sizeof(shader.hash.HashDigest));
        file.write(reinterpret_cast<const char*>(bytecode), bytecodeLength);
    }
    file.close();

    return true;
}

bool CheckCompiledShadersUpToDate()
{
    const auto engineShadersPath{ GetEngineShadersPath() };
    if (!std::filesystem::exists(engineShadersPath)) return false;
    const auto lastCompileTime{ std::filesystem::last_write_time(engineShadersPath) };

    for (const auto& entry : std::filesystem::directory_iterator{ SHADERS_SOURCE_PATH })
    {
        if (entry.last_write_time() > lastCompileTime) return false;
    }

    return true;
}

} // anonymous namespace

std::unique_ptr<u8[]> 
CompileShader(ShaderFileInfo info, u8* code, u32 codeSize, Vec<std::wstring>& extraArgs, bool includeErrorsAndDisassembly)
{
    assert(!info.file && info.entryPoint && code && codeSize);

    return PackShader(ShaderCompiler{}.Compile(code, codeSize, info.type, info.entryPoint, extraArgs), includeErrorsAndDisassembly);
}

std::unique_ptr<u8[]> 
CompileShader(ShaderFileInfo info, const char* path, Vec<std::wstring>& extraArgs, bool includeErrorsAndDisassembly)
{
    assert(info.entryPoint && info.file);

	std::filesystem::path fullPath{ path };
	fullPath += info.file;
	if (!std::filesystem::exists(fullPath)) return {};

    return PackShader(ShaderCompiler{}.Compile(info, fullPath, extraArgs), includeErrorsAndDisassembly);
}

bool 
CompileEngineShaders()
{
    if (CheckCompiledShadersUpToDate()) return true;
    Vec<DxcCompiledShader> shaders;

    std::filesystem::path path{};
    ShaderCompiler compiler{};

    for (u32 i{ 0 }; i < EngineShader::Count; ++i)
    {
        auto& file{ ENGINE_SHADER_FILES[i] };
        path = SHADERS_SOURCE_PATH;
        path += file.info.file;

        Vec<std::wstring> extraArgs{};
        if (file.id == EngineShader::CalculateGridFrustumsCS || file.id == EngineShader::LightCullingCS)
        {
            // TODO: get TILE_SIZE value from d3d12
            extraArgs.emplace_back(L"-D");
            extraArgs.emplace_back(L"TILE_SIZE=32");
        }

        DxcCompiledShader compiledShader{ compiler.Compile(file.info, path, extraArgs) };
        if (compiledShader.bytecode && compiledShader.bytecode->GetBufferPointer() && compiledShader.bytecode->GetBufferSize())
        {
            shaders.emplace_back(std::move(compiledShader));
        }
        else
        {
            return false;
        }
    }

    return SaveCompiledShaders(shaders);
}