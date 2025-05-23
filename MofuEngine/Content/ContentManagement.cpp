#include "ContentManagement.h"
#include "Graphics/Renderer.h"
#include "Utilities/Logger.h"

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

void 
GeneratePrimitiveMeshAsset(PrimitiveMeshInfo info)
{
    MeshGroupData data{};
	GeneratePrimitiveMesh(info, data);
	SaveGeometry(data, std::filesystem::path{ ASSET_BASE_DIRECTORY_PATH } / "Generated" / "plane.geom");
}

void
SaveGeometry(const MeshGroupData& data, std::filesystem::path path)
{
    assert(data.Buffer && data.BufferSize);
    std::ofstream file{ path, std::ios::out | std::ios::binary };
    if (!file) 
    {
		log::Error("Failed to open file for writing: %s", path.string().c_str());
		return;
    }

	file.write(GEOMETRY_SERIALIZED_ASSET_VERSION, SERIALIZED_ASSET_VERSION_LENGTH);
    file.write(reinterpret_cast<const char*>(data.Buffer), data.BufferSize);
    file.close();
}

void
ReadAssetFile(std::filesystem::path path, std::unique_ptr<u8[]>& dataOut, u64& sizeOut, AssetType::type type)
{
    //TODO: can assert extension here 
    //assert(std::filesystem::path::has_extension());

    if (!std::filesystem::exists(path))
    {
		log::Error("File does not exist: %s", path.string().c_str());
		return;
    }

	std::ifstream file{ path, std::ios::in | std::ios::binary };
    if (!file)
    {
		log::Error("Failed to open file for reading: %s", path.string().c_str());
        return;
    }

    sizeOut = std::filesystem::file_size(path) - SERIALIZED_ASSET_VERSION_LENGTH;
	assert(sizeOut != 0);

	char version[SERIALIZED_ASSET_VERSION_LENGTH + 1]{};
	file.read(version, SERIALIZED_ASSET_VERSION_LENGTH);
    if (std::string{ version } != ASSET_SERIALIZED_FILE_VERSIONS[type])
    {
		log::Error("Invalid file version: %s", version);
		return;
    }

	dataOut = std::make_unique<u8[]>(sizeOut);
    file.read(reinterpret_cast<char*>(dataOut.get()), sizeOut);

	file.close();
}

void
ReadAssetFileNoVersion(std::filesystem::path path, std::unique_ptr<u8[]>& dataOut, u64& sizeOut, AssetType::type type)
{
    //TODO: can assert extension here 
    //assert(std::filesystem::path::has_extension());

    if (!std::filesystem::exists(path))
    {
        log::Error("File does not exist: %s", path.string().c_str());
        return;
    }

    std::ifstream file{ path, std::ios::in | std::ios::binary };
    if (!file)
    {
        log::Error("Failed to open file for reading: %s", path.string().c_str());
        return;
    }

    sizeOut = std::filesystem::file_size(path);
    assert(sizeOut != 0);

    dataOut = std::make_unique<u8[]>(sizeOut);
    file.read(reinterpret_cast<char*>(dataOut.get()), sizeOut);

    file.close();
}

}
