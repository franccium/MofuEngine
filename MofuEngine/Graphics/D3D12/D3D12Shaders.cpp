#include "D3D12Shaders.h"
#include "Content/ContentManagement.h"
#include "Content/ShaderCompilation.h"
#include "Content/ResourceCreation.h"

namespace mofu::graphics::d3d12::shaders {
namespace {
mofu::shaders::CompiledShaderPtr _engineShaders[EngineShader::Count]{};
mofu::shaders::CompiledShaderPtr _engineShaders_Debug[EngineShader::Count]{};

#if SHADER_HOT_RELOAD_ENABLED
Array<std::unique_ptr<u8[]>> _engineShadersBlobs{};
Array<std::unique_ptr<u8[]>> _engineShadersBlobs_Debug{};
#else
// a chunk of memory with all compiled engine shaders
// an array of shader byte code consisting of a u64 size and an array of bytes
std::unique_ptr<u8[]> _engineShadersBlob{};
std::unique_ptr<u8[]> _engineShadersBlob_Debug{};
#endif


#if SHADER_HOT_RELOAD_ENABLED
bool
LoadEngineShaders()
{
    assert(_engineShadersBlobs.empty() && _engineShadersBlobs_Debug.empty());
	_engineShadersBlobs.Initialize(EngineShader::Count);

    u64 totalSize{ 0 };
    bool result{ content::LoadEngineShaders(_engineShadersBlobs, totalSize) };
    if (!result) return false;
    assert(_engineShadersBlobs.size() == EngineShader::Count && totalSize != 0);

    for(u32 i{0}; i < EngineShader::Count; ++i)
    {
        mofu::shaders::CompiledShaderPtr& shader{ _engineShaders[i] };
        assert(!shader);

        shader = reinterpret_cast<const mofu::shaders::CompiledShaderPtr>(_engineShadersBlobs[i].get());
    }

    // load debug shaders
    _engineShadersBlobs_Debug.Initialize(EngineDebugShader::Count);
    totalSize = 0;
    result = content::LoadDebugEngineShaders(_engineShadersBlobs_Debug, totalSize);
    if (!result) return false;
    assert(_engineShadersBlobs_Debug.size() == EngineDebugShader::Count && totalSize != 0);

    for (u32 i{ 0 }; i < EngineDebugShader::Count; ++i)
    {
        mofu::shaders::CompiledShaderPtr& shader{ _engineShaders_Debug[i] };
        assert(!shader);

        shader = reinterpret_cast<const mofu::shaders::CompiledShaderPtr>(_engineShadersBlobs_Debug[i].get());
    }

    return result;
}
#else
bool
LoadEngineShaders()
{
    assert(!_engineShadersBlob);

    u64 totalSize{ 0 };
    bool result{ content::LoadEngineShaders(_engineShadersBlob, totalSize) };
    if (!result) return false;

    assert(_engineShadersBlob && totalSize != 0);

    u64 offset{ 0 };
    u32 index{ 0 };
    while (offset < totalSize)
    {
        mofu::shaders::CompiledShaderPtr& shader{ _engineShaders[index] };
        assert(!shader);
        result &= (index < EngineShader::Count && !shader);
        if (!result) break;

        shader = reinterpret_cast<const mofu::shaders::CompiledShaderPtr>(&_engineShadersBlob[offset]);
        offset += shader->GetBufferSize();
        ++index;
    }
    assert(index == EngineShader::Count && offset == totalSize);

    // load debug shaders
    totalSize = 0;
    result = content::LoadDebugEngineShaders(_engineShadersBlob_Debug, totalSize);
    offset = 0;
    index = 0;
    if (!result) return false;
    while (offset < totalSize)
    {
        mofu::shaders::CompiledShaderPtr& shader{ _engineShaders_Debug[index] };
        assert(!shader);

        shader = reinterpret_cast<const mofu::shaders::CompiledShaderPtr>(&_engineShadersBlob_Debug[offset]);
        offset += shader->GetBufferSize();
        ++index;
    }
    assert(index == EngineDebugShader::Count && offset == totalSize);

    return result;
}
#endif

} // anonymous namespace

bool 
Initialize()
{
    return LoadEngineShaders();
}

void 
Shutdown()
{
#if SHADER_HOT_RELOAD_ENABLED
    for (u32 i{ 0 }; i < EngineShader::Count; ++i)
    {
        _engineShaders[i] = {};
        _engineShadersBlobs[i].reset();
        _engineShadersBlobs_Debug[i].reset();
    }
#else
    for (u32 i{ 0 }; i < EngineShader::Count; ++i)
    {
        engineShaders[i] = {};
    }
    _engineShadersBlob.reset();
    _engineShadersBlob_Debug.reset();
#endif
}

void 
ReloadShader(EngineShader::ID id)
{
	content::LoadEngineShader(_engineShadersBlobs[id], id);
    assert(_engineShadersBlobs[id]);
	_engineShaders[id] = reinterpret_cast<const mofu::shaders::CompiledShaderPtr>(_engineShadersBlobs[id].get());
}


D3D12_SHADER_BYTECODE 
GetEngineShader(EngineShader::ID id)
{
    assert(id < EngineShader::Count);
    const mofu::shaders::CompiledShaderPtr shader{ _engineShaders[id] };
    assert(shader && shader->BytecodeSize());
    return { shader->Bytecode(), shader->BytecodeSize() };
}

D3D12_SHADER_BYTECODE 
GetDebugEngineShader(EngineDebugShader::ID id)
{
    assert(id < EngineDebugShader::Count);
    const mofu::shaders::CompiledShaderPtr shader{ _engineShaders_Debug[id] };
    assert(shader && shader->BytecodeSize());
    return { shader->Bytecode(), shader->BytecodeSize() };
}

}
