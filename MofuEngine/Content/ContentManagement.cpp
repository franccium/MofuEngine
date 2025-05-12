#include "ContentManagement.h"
#include "Graphics/Renderer.h"

// TODO: file system
#include <fstream>
#include <filesystem>
#include <Windows.h>

namespace mofu::content {
namespace {

bool
ReadFile(std::filesystem::path path, std::unique_ptr<u8[]>& outData, u64& outFileSize)
{ 
    if (!std::filesystem::exists(path)) return false;
    outFileSize = std::filesystem::file_size(path);
    assert(outFileSize != 0);

    outData = std::make_unique<u8[]>(outFileSize);
    std::ifstream file{ path, std::ios::in | std::ios::binary };
    if (!file || !file.read((char*)outData.get(), outFileSize))
    {
        file.close();
        return false;
    }
    file.close();
    return true;
}

} // anonymous namespace

bool
LoadEngineShaders(std::unique_ptr<u8[]>& shaders, u64& size)
{
    return ReadFile(std::filesystem::path{ graphics::GetEngineShadersPath() }, shaders, size);
}

}
