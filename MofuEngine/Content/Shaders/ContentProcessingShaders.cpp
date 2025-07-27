#include "ContentProcessingShaders.h"
#include "Content/ResourceCreation.h"
#include "Content/ShaderCompilation.h"

namespace mofu::content::shaders {
namespace {
CompiledShaderPtr contentShaders[ContentShader::Count]{};
// a chunk of memory with all compiled content shaders
// an array of shader byte code consisting of a u64 size and an array of bytes
std::unique_ptr<u8[]> contentShadersBlob{};


bool 
LoadShaders()
{
    assert(!contentShadersBlob);
    std::filesystem::path path{ CONTENT_SHADERS_COMPILED_PATH };
    assert(std::filesystem::exists(path));

    u64 totalSize{ 0 };
    bool result{ ReadFileToByteBuffer(path, contentShadersBlob, totalSize) };
    if (!result) return false;

    assert(contentShadersBlob && totalSize != 0);

    u64 offset{ 0 };
    u32 index{ 0 };
    while (offset < totalSize)
    {
        content::CompiledShaderPtr& shader{ contentShaders[index] };
        assert(!shader);
        result &= (index < ContentShader::Count && !shader);
        if (!result) break;

        shader = reinterpret_cast<const content::CompiledShaderPtr>(&contentShadersBlob[offset]);
        offset += shader->GetBufferSize();
        ++index;
    }
    assert(index == ContentShader::Count && offset == totalSize);

    return result;
}

bool CheckCompiledShadersUpToDate()
{
    std::filesystem::path path{ CONTENT_SHADERS_COMPILED_PATH };
    if (!std::filesystem::exists(path)) return false;
    const auto lastCompileTime{ std::filesystem::last_write_time(path) };

    for (const auto& entry : std::filesystem::directory_iterator{ CONTENT_SHADERS_SOURCE_PATH })
    {
        if (entry.last_write_time() > lastCompileTime) return false;
    }

    return true;
}

bool
CompileContentShaders()
{
    //TODO:if (CheckCompiledShadersUpToDate()) return true;
    return mofu::shaders::CompileContentProcessingShaders();
}

}

bool 
Initialize()
{
	return CompileContentShaders() && LoadShaders();
}

void 
Shutdown()
{
    for (u32 i{ 0 }; i < ContentShader::Count; ++i)
    {
        contentShaders[i] = {};
    }
    contentShadersBlob.reset();
}

//ShaderBytecode
//GetContentShader(ContentShader::ID id)
//{
//    assert(id < EngineShader::Count);
//    const CompiledShaderPtr shader{ contentShaders[id] };
//    assert(shader && shader->BytecodeSize());
//    return { shader->Bytecode(), shader->BytecodeSize() };
//}

D3D12_SHADER_BYTECODE
GetContentShader(ContentShader::ID id)
{
    assert(id < EngineShader::Count);
    const CompiledShaderPtr shader{ contentShaders[id] };
    assert(shader && shader->BytecodeSize());
    return { shader->Bytecode(), shader->BytecodeSize() };
}

}