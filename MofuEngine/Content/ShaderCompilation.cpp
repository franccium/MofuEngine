#include "ShaderCompilation.h"
#include <filesystem>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <wrl.h>
#include <dxcapi.h> 
#include <d3d12shader.h>
#include <d3d11shader.h>
#include <fstream>
#include "EngineShaders.h"
#include "Graphics/Renderer.h"
#include "ContentManagement.h"
#include "Content/ResourceCreation.h"

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

namespace mofu::shaders {
namespace {
constexpr const char* SHADERS_SOURCE_PATH{ "../MofuEngine/Graphics/D3D12/Shaders/" };
constexpr const char* SHADERS_DEBUG_SOURCE_PATH{ "../MofuEngine/Graphics/D3D12/Shaders/" };

constexpr std::wstring
c_to_wstring(const char* c)
{
    std::string str{ c };
    return{ str.begin(), str.end() };
}

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

    DxcCompiledShader Compile(IDxcBlobEncoding* sourceBlob, Vec<std::wstring> compilerArgs, [[maybe_unused]] bool debug)
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

    DxcCompiledShader Compile(u8* data, u32 dataSize, ShaderType::Type type,
        const char* entryPoint, Vec<std::wstring>& extraArgs, bool debug)
    {
        assert(data && dataSize && !(type != ShaderType::Library && !entryPoint));
        assert(_compiler && _utils && _includeHandler);
        assert(type < ShaderType::CountWithLibrary);

        HRESULT hr{ S_OK };
        ComPtr<IDxcBlobEncoding> sourceBlob{ nullptr };
        DXCall(hr = _utils->CreateBlob(data, dataSize, 0, &sourceBlob));
        if (FAILED(hr)) return {};
        assert(sourceBlob && sourceBlob->GetBufferSize());

        ShaderFileInfo info{};
        info.EntryPoint = entryPoint;
        info.Type = type;

        OutputDebugStringA("Compiling ");
        OutputDebugStringA(info.EntryPoint);
        OutputDebugStringA("\n");

        return Compile(sourceBlob.Get(), GetCompilerArgs(info, extraArgs, debug), debug);
    }

    DxcCompiledShader Compile(ShaderFileInfo info, std::filesystem::path fullPath, Vec<std::wstring>& extraArgs, bool debug)
    {
        assert(_compiler && _utils && _includeHandler);
        HRESULT hr{ S_OK };

        ComPtr<IDxcBlobEncoding> sourceBlob{ nullptr };
        DXCall(hr = _utils->LoadFile(fullPath.c_str(), nullptr, &sourceBlob));
        if (FAILED(hr)) return {};
        assert(sourceBlob && sourceBlob->GetBufferSize());

        OutputDebugStringA("Compiling ");
        OutputDebugStringA(info.File);
        OutputDebugStringA(" : ");
        OutputDebugStringA(info.Type != ShaderType::Library ? info.EntryPoint : "[Libary]");
        OutputDebugStringA("\n");

        return Compile(sourceBlob.Get(), GetCompilerArgs(info, extraArgs, debug), debug);
    }

    DxcCompiledShader CompileD3D11(ShaderFileInfo info, std::filesystem::path fullPath, Vec<std::wstring>& extraArgs)
    {
        assert(_compiler && _utils && _includeHandler);
        HRESULT hr{ S_OK };

        ComPtr<IDxcBlobEncoding> sourceBlob{ nullptr };
        DXCall(hr = _utils->LoadFile(fullPath.c_str(), nullptr, &sourceBlob));
        if (FAILED(hr)) return {};
        assert(sourceBlob && sourceBlob->GetBufferSize());

        OutputDebugStringA("Compiling ");
        OutputDebugStringA(info.File);
        OutputDebugStringA(" : ");
        OutputDebugStringA(info.EntryPoint);
        OutputDebugStringA("\n");

        return Compile(sourceBlob.Get(), GetContentProcessingCompilerArgs(info, extraArgs), false);
    }

private:
    constexpr static u8 DEFAULT_COMPILER_ARGS_COUNT{ 13 };
    Vec<std::wstring> GetCompilerArgs(const ShaderFileInfo& info, Vec<std::wstring>& extraArgs, bool debug)
    {
        Vec<std::wstring> args((u64)DEFAULT_COMPILER_ARGS_COUNT + extraArgs.size());
        if (info.File) args.emplace_back(c_to_wstring(info.File));
        if (info.Type != ShaderType::Library)
        {
            args.emplace_back(L"-E");
            args.emplace_back(c_to_wstring(info.EntryPoint));
        }
        args.emplace_back(L"-T");
        args.emplace_back(c_to_wstring(SHADER_PROFILES[(u32)info.Type]));
        args.emplace_back(L"-I");
        args.emplace_back(c_to_wstring(debug ? SHADERS_DEBUG_SOURCE_PATH : SHADERS_SOURCE_PATH));
        if (debug)
        {
            extraArgs.emplace_back(L"-D");
            extraArgs.emplace_back(L"DEBUG=1");
        }
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

    Vec<std::wstring> GetContentProcessingCompilerArgs(const ShaderFileInfo& info, Vec<std::wstring>& extraArgs)
    {
        Vec<std::wstring> args((u64)DEFAULT_COMPILER_ARGS_COUNT + extraArgs.size());
        if (info.File) args.emplace_back(c_to_wstring(info.File));
        args.emplace_back(L"-E");
        args.emplace_back(c_to_wstring(info.EntryPoint));
        args.emplace_back(L"-T");
        args.emplace_back(c_to_wstring(SHADER_PROFILES[(u32)info.Type]));
        args.emplace_back(L"-I");
        args.emplace_back(c_to_wstring(content::CONTENT_SHADERS_SOURCE_PATH));

        args.emplace_back(L"-flegacy-macro-expansion");
        args.emplace_back(L"-flegacy-resource-reservation");
        args.emplace_back(L"-HV");
        args.emplace_back(L"2016");

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

	constexpr static const char* SHADER_PROFILES[]{ "vs_6_7", "ps_6_7", "ds_6_7", "hs_6_7", "gs_6_7", "cs_6_7", "as_6_7", "ms_6_7", "lib_6_7"};
	static_assert(_countof(SHADER_PROFILES) == ShaderType::CountWithLibrary);

	ComPtr<IDxcCompiler3> _compiler{ nullptr };
	ComPtr<IDxcUtils> _utils{ nullptr };
	ComPtr<IDxcIncludeHandler> _includeHandler{ nullptr };
};

std::unique_ptr<u8[]>
PackShader(DxcCompiledShader shader, bool includeErrorsAndDissasembly)
{
    if (shader.bytecode && shader.bytecode->GetBufferPointer() && shader.bytecode->GetBufferSize())
    {
        static_assert(CompiledShader::HASH_LENGTH == _countof(DxcShaderHash::HashDigest));
        const u64 extraSize{ includeErrorsAndDissasembly
            ? sizeof(u64) + sizeof(u64) + shader.errors->GetStringLength() + shader.assembly->GetStringLength()
            : 0 };
        const u64 bufferSize{ sizeof(u64) + CompiledShader::HASH_LENGTH + shader.bytecode->GetBufferSize() + extraSize };

        std::unique_ptr<u8[]> buffer{ std::make_unique<u8[]>(bufferSize) };
        util::BlobStreamWriter blob{ buffer.get(), bufferSize };

        blob.Write(shader.bytecode->GetBufferSize());
        blob.WriteBytes(shader.hash.HashDigest, CompiledShader::HASH_LENGTH);
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

const std::filesystem::path 
GetDebugEngineShadersPath()
{
    return graphics::GetDebugEngineShadersPath(graphics::GraphicsPlatform::Direct3D12);
}

// TODO: figure out how not to repeat this code so much
bool
SaveCompiledEngineShaders(const Vec<DxcCompiledShader>& shaders)
{
    assert(shaders.size() == EngineShader::Count);
    std::filesystem::create_directories(std::filesystem::path{ GetEngineShadersPath() }.parent_path());
    for(u32 i{0}; i < EngineShader::Count; ++i)
    {
		std::filesystem::path savePath{ graphics::GetEngineShaderPath((EngineShader::ID)i) };
        std::ofstream file(savePath, std::ios::out | std::ios::binary);
        /*if (!std::filesystem::exists(savePath) || !file || !file.is_open())
        {
            return false;
        }*/
		const DxcCompiledShader& shader{ shaders[i] };

        void* const bytecode{ shader.bytecode->GetBufferPointer() };
        const u64 bytecodeLength{ shader.bytecode->GetBufferSize() };
        file.write(reinterpret_cast<const char*>(&bytecodeLength), sizeof(bytecodeLength));
        file.write(reinterpret_cast<const char*>(shader.hash.HashDigest), sizeof(shader.hash.HashDigest));
        file.write(reinterpret_cast<const char*>(bytecode), bytecodeLength);
	}

    return true;
}
bool
SaveCompiledEngineShader(const DxcCompiledShader& shader, const std::filesystem::path& savePath)
{
    std::ofstream file(savePath, std::ios::out | std::ios::binary);
    if (!std::filesystem::exists(savePath) || !file || !file.is_open())
    {
        return false;
    }

    void* const bytecode{ shader.bytecode->GetBufferPointer() };
    const u64 bytecodeLength{ shader.bytecode->GetBufferSize() };
    file.write(reinterpret_cast<const char*>(&bytecodeLength), sizeof(bytecodeLength));
    file.write(reinterpret_cast<const char*>(shader.hash.HashDigest), sizeof(shader.hash.HashDigest));
    file.write(reinterpret_cast<const char*>(bytecode), bytecodeLength);

    return true;
}
bool
SaveCompiledEngineDebugShaders(const Vec<DxcCompiledShader>& shaders)
{
    assert(shaders.size() == EngineDebugShader::Count);
    std::filesystem::create_directories(std::filesystem::path{ GetDebugEngineShadersPath() }.parent_path());
    for (u32 i{ 0 }; i < EngineDebugShader::Count; ++i)
    {
        std::filesystem::path savePath{ graphics::GetDebugEngineShaderPath((EngineDebugShader::ID)i) };
        std::ofstream file(savePath, std::ios::out | std::ios::binary);
        if (!std::filesystem::exists(savePath) || !file || !file.is_open())
        {
            return false;
        }
        const DxcCompiledShader& shader{ shaders[i] };

        void* const bytecode{ shader.bytecode->GetBufferPointer() };
        const u64 bytecodeLength{ shader.bytecode->GetBufferSize() };
        file.write(reinterpret_cast<const char*>(&bytecodeLength), sizeof(bytecodeLength));
        file.write(reinterpret_cast<const char*>(shader.hash.HashDigest), sizeof(shader.hash.HashDigest));
        file.write(reinterpret_cast<const char*>(bytecode), bytecodeLength);
    }

    return true;
}

bool
SaveCompiledShaders(const Array<DxcCompiledShader>& shaders, const std::filesystem::path& savePath)
{
    std::filesystem::create_directories(savePath.parent_path());
    std::ofstream file(savePath, std::ios::out | std::ios::binary);
    if (!std::filesystem::exists(savePath) || !file || !file.is_open())
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
CompileShader(ShaderFileInfo info, u8* code, u32 codeSize, Vec<std::wstring>& extraArgs, bool debug, bool includeErrorsAndDisassembly)
{
    assert(!info.File && info.EntryPoint && code && codeSize);

    return PackShader(ShaderCompiler{}.Compile(code, codeSize, info.Type, info.EntryPoint, extraArgs, debug), includeErrorsAndDisassembly);
}

std::unique_ptr<u8[]> 
CompileShader(ShaderFileInfo info, const char* path, Vec<std::wstring>& extraArgs, bool debug, bool includeErrorsAndDisassembly)
{
    assert(info.EntryPoint && info.File);

	std::filesystem::path fullPath{ path };
	fullPath += info.File;
	if (!std::filesystem::exists(fullPath)) return {};

    return PackShader(ShaderCompiler{}.Compile(info, fullPath, extraArgs, debug), includeErrorsAndDisassembly);
}

DxcCompiledShader
CompileEngineShader(EngineShader::ID shaderID, ShaderCompiler& compiler)
{
    auto& file{ ENGINE_SHADER_FILES[shaderID] };
    std::filesystem::path path{ SHADERS_SOURCE_PATH };
    path += file.Info.File;

    Vec<std::wstring> extraArgs{};
    if (file.ID == EngineShader::CalculateGridFrustumsCS || file.ID == EngineShader::LightCullingCS)
    {
        // TODO: get TILE_SIZE value from d3d12
        extraArgs.emplace_back(L"-D");
        extraArgs.emplace_back(L"TILE_SIZE=32");
    }
    //TODO: get all the engine defines from a common place
#if RAYTRACING
    extraArgs.emplace_back(L"-D");
    extraArgs.emplace_back(L"RAYTRACING=1");
#endif

    DxcCompiledShader compiledShader{ compiler.Compile(file.Info, path, extraArgs, false) };
    return compiledShader;
}

DxcCompiledShader
CompileEngineShaderDebug(EngineDebugShader::ID shaderID, ShaderCompiler& compiler)
{
    auto& file{ ENGINE_DEBUG_SHADER_FILES[shaderID] };
    std::filesystem::path path{ SHADERS_DEBUG_SOURCE_PATH };
    path += file.Info.File;

    Vec<std::wstring> extraArgs{};
    //TODO: get all the engine defines from a common place
#if RAYTRACING
    extraArgs.emplace_back(L"-D");
    extraArgs.emplace_back(L"RAYTRACING=1");
#endif

    DxcCompiledShader compiledShader{ compiler.Compile(file.Info, path, extraArgs, true) };
    return compiledShader;
}

#if SHADER_HOT_RELOAD_ENABLED
bool
CompileEngineShaders()
{
    //if (CheckCompiledShadersUpToDate()) return true;
    /*Array<DxcCompiledShader> shaders{};
    Array<DxcCompiledShader> debugShaders{};*/

    std::filesystem::path path{};
    ShaderCompiler compiler{};

    for (u32 i{ 0 }; i < EngineShader::Count; ++i)
    {
		DxcCompiledShader shader{ CompileEngineShader((EngineShader::ID)i, compiler) };
        if(!shader.bytecode || !shader.bytecode->GetBufferPointer() || !shader.bytecode->GetBufferSize()) return false;
        SaveCompiledEngineShader(shader, graphics::GetEngineShaderPath((EngineShader::ID)i));
        //if (!shaders[i].bytecode || !shaders[i].bytecode->GetBufferPointer() || !shaders[i].bytecode->GetBufferSize()) return false;
    }

    for (u32 i{ 0 }; i < EngineDebugShader::Count; ++i)
    {
        DxcCompiledShader shader{ CompileEngineShaderDebug((EngineDebugShader::ID)i, compiler) };
        if (!shader.bytecode || !shader.bytecode->GetBufferPointer() || !shader.bytecode->GetBufferSize()) return false;
        SaveCompiledEngineShader(shader, graphics::GetDebugEngineShaderPath((EngineDebugShader::ID)i));
    }

    return true;
}
#else
bool 
CompileEngineShaders()
{
    if (CheckCompiledShadersUpToDate()) return true;
    Vec<DxcCompiledShader> shaders;
    Vec<DxcCompiledShader> debugShaders;

    std::filesystem::path path{};
    ShaderCompiler compiler{};

    for (u32 i{ 0 }; i < EngineShader::Count; ++i)
    {
        auto& file{ ENGINE_SHADER_FILES[i] };
        path = SHADERS_SOURCE_PATH;
        path += file.Info.File;

        Vec<std::wstring> extraArgs{};
        if (file.ID == EngineShader::CalculateGridFrustumsCS || file.ID == EngineShader::LightCullingCS)
        {
            // TODO: get TILE_SIZE value from d3d12
            extraArgs.emplace_back(L"-D");
            extraArgs.emplace_back(L"TILE_SIZE=32");
        }
		//TODO: get all the engine defines from a common place
#if RAYTRACING
		extraArgs.emplace_back(L"-D");
		extraArgs.emplace_back(L"RAYTRACING=1");
#endif

        DxcCompiledShader compiledShader{ compiler.Compile(file.Info, path, extraArgs, false) };
        if (compiledShader.bytecode && compiledShader.bytecode->GetBufferPointer() && compiledShader.bytecode->GetBufferSize())
        {
            shaders.emplace_back(std::move(compiledShader));
        }
        else
        {
            return false;
        }
    }

    for (u32 i{ 0 }; i < EngineDebugShader::Count; ++i)
    {
        auto& file{ ENGINE_DEBUG_SHADER_FILES[i] };
        path = SHADERS_DEBUG_SOURCE_PATH;
        path += file.Info.File;

        Vec<std::wstring> extraArgs{};

        DxcCompiledShader compiledShader{ compiler.Compile(file.Info, path, extraArgs, true) };
        if (compiledShader.bytecode && compiledShader.bytecode->GetBufferPointer() && compiledShader.bytecode->GetBufferSize())
        {
            debugShaders.emplace_back(std::move(compiledShader));
        }
        else
        {
            return false;
        }
    }

    return SaveCompiledShaders(shaders, GetEngineShadersPath()) && SaveCompiledShaders(debugShaders, GetDebugEngineShadersPath());
}
#endif

bool
CompileContentProcessingShaders()
{
    Array<DxcCompiledShader> shaders{ content::ContentShader::Count };

    std::filesystem::path path{ content::CONTENT_SHADERS_SOURCE_PATH };
    ShaderCompiler compiler{};

    for (u32 i{ 0 }; i < content::ContentShader::Count; ++i)
    {
        auto& file{ content::CONTENT_SHADER_FILES[i] };
        path.replace_filename(file.Info.File);

        Vec<std::wstring> extraArgs{};
        extraArgs.emplace_back(L"-I");
        extraArgs.emplace_back(c_to_wstring(content::CONTENT_SHADERS_SOURCE_PATH));

        DxcCompiledShader compiledShader{ compiler.Compile(file.Info, path, extraArgs, false) };
        if (compiledShader.bytecode && compiledShader.bytecode->GetBufferPointer() && compiledShader.bytecode->GetBufferSize())
        {
            shaders[i] = compiledShader;
        }
        else
        {
            return false;
        }
    }

    return SaveCompiledShaders(shaders, content::CONTENT_SHADERS_COMPILED_PATH);
}

void 
UpdateHotReload()
{
    EngineShader::ID shaderID{};
    if (!std::filesystem::exists(SHADERS_SOURCE_PATH)) return;
    ShaderCompiler compiler{};

    /*for (const auto& entry : std::filesystem::directory_iterator{ SHADERS_SOURCE_PATH })
    {
        if (entry.last_write_time() > lastCompileTime) return false;
    }*/
    std::filesystem::path shadersSourcePath{ SHADERS_SOURCE_PATH };
    std::filesystem::path shadersCompiledPath{ GetEngineShadersPath() };
    shadersCompiledPath.remove_filename();

    std::filesystem::path shaderPath{ shadersSourcePath / ENGINE_SHADER_FILES[EngineShader::RayTracingLib].Info.File };
    //std::filesystem::path compiledShaderPath{ shadersCompiledPath / graphics::GetEngineShaderPath(EngineShader::RayTracingLib) };
    std::filesystem::path compiledShaderPath{ graphics::GetEngineShaderPath(EngineShader::RayTracingLib) };
	std::filesystem::directory_entry entry{ shaderPath };
    const auto lastSourceUpdateTime{ std::chrono::time_point_cast<std::chrono::seconds>(std::filesystem::last_write_time(shaderPath))};
    const auto lastCompileTime{ std::chrono::time_point_cast<std::chrono::seconds>(std::filesystem::last_write_time(compiledShaderPath))};

    if (lastSourceUpdateTime > lastCompileTime + std::chrono::seconds(1))
    {
        //TODO: maybe dont write to a file, just use the blob? - that could be a problem for the next engine run though
        DxcCompiledShader shader{ CompileEngineShader(EngineShader::RayTracingLib, compiler) };
		SaveCompiledEngineShader(shader, graphics::GetEngineShaderPath(EngineShader::RayTracingLib));
        graphics::OnShadersRecompiled(EngineShader::ID::RayTracingLib);
    }
}
}