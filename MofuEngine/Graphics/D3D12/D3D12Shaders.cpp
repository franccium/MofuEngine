#include "D3D12Shaders.h"
#include "Content/ContentManagement.h"
#include "Content/ShaderCompilation.h"
#include "Content/ResourceCreation.h"

namespace mofu::graphics::d3d12::shaders {
namespace {
content::CompiledShaderPtr engineShaders[EngineShader::Count]{};
content::CompiledShaderPtr engineShaders_Debug[EngineShader::Count]{};

// a chunk of memory with all compiled engine shaders
// an array of shader byte code consisting of a u64 size and an array of bytes
std::unique_ptr<u8[]> engineShadersBlob{};
std::unique_ptr<u8[]> engineShadersBlob_Debug{};

bool 
LoadEngineShaders()
{
    assert(!engineShadersBlob);

    u64 totalSize{ 0 };
    bool result{ content::LoadEngineShaders(engineShadersBlob, totalSize) };
    if (!result) return false;

    assert(engineShadersBlob && totalSize != 0);

    u64 offset{ 0 };
    u32 index{ 0 };
    while (offset < totalSize)
    {
        content::CompiledShaderPtr& shader{ engineShaders[index] };
        assert(!shader);
        result &= (index < EngineShader::Count && !shader);
        if (!result) break;

        shader = reinterpret_cast<const content::CompiledShaderPtr>(&engineShadersBlob[offset]);
        offset += shader->GetBufferSize();
        ++index;
    }
    assert(index == EngineShader::Count && offset == totalSize);

    // load debug shaders
    totalSize = 0;
    result = content::LoadDebugEngineShaders(engineShadersBlob_Debug, totalSize);
    offset = 0;
    index = 0;
    if (!result) return false;
    while (offset < totalSize)
    {
        content::CompiledShaderPtr& shader{ engineShaders_Debug[index] };
        assert(!shader);

        shader = reinterpret_cast<const content::CompiledShaderPtr>(&engineShadersBlob_Debug[offset]);
        offset += shader->GetBufferSize();
        ++index;
    }
    assert(index == EngineDebugShader::Count && offset == totalSize);

    return result;
}

} // anonymous namespace

bool 
Initialize()
{
    return LoadEngineShaders();
}

void 
Shutdown()
{
    for (u32 i{ 0 }; i < EngineShader::Count; ++i)
    {
        engineShaders[i] = {};
    }
    engineShadersBlob.reset();
}


D3D12_SHADER_BYTECODE 
GetEngineShader(EngineShader::id id)
{
    assert(id < EngineShader::Count);
    const content::CompiledShaderPtr shader{ engineShaders[id] };
    assert(shader && shader->BytecodeSize());
    return { shader->Bytecode(), shader->BytecodeSize() };
}

D3D12_SHADER_BYTECODE 
GetDebugEngineShader(EngineDebugShader::id id)
{
    assert(id < EngineDebugShader::Count);
    const content::CompiledShaderPtr shader{ engineShaders_Debug[id] };
    assert(shader && shader->BytecodeSize());
    return { shader->Bytecode(), shader->BytecodeSize() };
}

}
