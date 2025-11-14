#include "ContentManagement.h"
#include "ContentUtils.h"
#include "Graphics/Renderer.h"
#include "Utilities/Logger.h"

#include "Graphics/Renderer.h"
#include "Content/ResourceCreation.h"
#include "Content/ShaderCompilation.h"
#include "AssetImporter.h"
#include "Editor/Project/Project.h"
#include "EditorContentManager.h"

// TODO: file system
#include <fstream>
#include <filesystem>
#include <Windows.h>

namespace mofu::content {
namespace {
id_t defaultMeshID{};
id_t defaultMaterialID{};
id_t defaultVSID{};
id_t defaultPSID{};
id_t defaultTexturedPSID{};
id_t skyboxMaterialID{};

void
CreateDefaultShaders()
{
    shaders::ShaderFileInfo info{};
    info.File = "TestShader.hlsl";
    info.EntryPoint = "TestShaderVS";
    info.Type = shaders::ShaderType::Vertex;
    const char* shaderPath{ "..\\ExampleApp\\" };

    std::wstring defines[]{ L"ELEMENTS_TYPE=1", L"ELEMENTS_TYPE=3" };
    Vec<u32> keys{};
    keys.emplace_back((u32)content::ElementType::StaticNormal);
    keys.emplace_back((u32)content::ElementType::StaticNormalTexture);

    Vec<std::wstring> extraArgs{};
    Vec<std::unique_ptr<u8[]>> vertexShaders{};
    Vec<const u8*> vertexShaderPtrs{};
    for (u32 i{ 0 }; i < keys.size(); ++i)
    {
        extraArgs.clear();
        extraArgs.emplace_back(L"-D");
        extraArgs.emplace_back(defines[i]);
        vertexShaders.emplace_back(std::move(shaders::CompileShader(info, shaderPath, extraArgs)));
        assert(vertexShaders.back().get());
        vertexShaderPtrs.emplace_back(vertexShaders.back().get());
    }

    info.EntryPoint = "TestShaderPS";
    info.Type = shaders::ShaderType::Pixel;
    Vec<std::unique_ptr<u8[]>> pixelShaders{};

    extraArgs.clear();
    pixelShaders.emplace_back(std::move((shaders::CompileShader(info, shaderPath, extraArgs))));
    assert(pixelShaders.back().get());
    Vec<const u8*> psShaderPtrs{};

    extraArgs.clear();
    std::vector<std::vector<std::wstring>> psDefines{
        {L"TEXTURED_MTL=1"},
        {L"TEXTURED_MTL=1", L"ALPHA_TEST=1"},
        {L"TEXTURED_MTL=1", L"ALPHA_BLEND=1"},
    };
    Vec<u32> psKeys{};
    psKeys.emplace_back((u32)graphics::MaterialType::Opaque);
    psKeys.emplace_back((u32)graphics::MaterialType::AlphaTested);
    psKeys.emplace_back((u32)graphics::MaterialType::AlphaBlended);
    for (u32 i{ 0 }; i < psKeys.size(); ++i)
    {
        extraArgs.clear();
        for (auto& s : psDefines[i])
        {
            extraArgs.emplace_back(L"-D");
            extraArgs.emplace_back(s);
        }

        pixelShaders.emplace_back(std::move(shaders::CompileShader(info, shaderPath, extraArgs)));
        assert(pixelShaders.back().get());
        psShaderPtrs.emplace_back(pixelShaders.back().get());
    }
    assert(pixelShaders.back().get());

    const u8* untexturedPSPtr{ pixelShaders[0].get() };
    const u32 untexturedKey{ (u32)graphics::MaterialType::Opaque };
    defaultVSID = content::AddShaderGroup(vertexShaderPtrs.data(), (u32)vertexShaderPtrs.size(), keys.data());
    defaultPSID = content::AddShaderGroup(&untexturedPSPtr, 1, &untexturedKey);

    defaultTexturedPSID = content::AddShaderGroup(psShaderPtrs.data(), (u32)psShaderPtrs.size(), psKeys.data());
}

void
CreateDefaultMaterial()
{
    assert(id::IsValid(defaultVSID) && id::IsValid(defaultPSID));

    graphics::MaterialInitInfo info{};
    info.ShaderIDs[shaders::ShaderType::Vertex] = defaultVSID;
    info.ShaderIDs[shaders::ShaderType::Pixel] = defaultPSID;
    info.Type = graphics::MaterialType::Opaque;
    skyboxMaterialID = content::CreateResourceFromBlob(&info, content::AssetType::Material);

    info.ShaderIDs[shaders::ShaderType::Vertex] = defaultVSID;
    info.ShaderIDs[shaders::ShaderType::Pixel] = defaultPSID;
    info.Type = graphics::MaterialType::Opaque;
    defaultMaterialID = content::CreateResourceFromBlob(&info, content::AssetType::Material);
    assets::PairAssetWithResource(assets::DEFAULT_MATERIAL_UNTEXTURED_HANDLE, defaultMaterialID, content::AssetType::Material);
}

} // anonymous namespace

id_t
GetDefaultMesh()
{
    return defaultMeshID;
}

id_t 
GetDefaultMaterial()
{
    return defaultMaterialID;
}

std::pair<id_t, id_t> 
GetDefaultPsVsShadersTextured()
{
    return std::pair<id_t, id_t>{ defaultVSID, defaultTexturedPSID };
}

std::pair<id_t, id_t>
GetDefaultPsVsShaders()
{
    return std::pair<id_t, id_t>{ defaultVSID , defaultPSID };
}

bool 
LoadEngineMeshes(std::filesystem::path defaultGeometryPath)
{
    assert(std::filesystem::exists(defaultGeometryPath));
    std::unique_ptr<u8[]> geometryBuffer;
    u64 size;
    ReadAssetFileNoVersion(defaultGeometryPath, geometryBuffer, size, content::AssetType::Mesh);
    assert(geometryBuffer.get());
    defaultMeshID = content::CreateResourceFromBlob(geometryBuffer.get(), content::AssetType::Mesh);
    return defaultMeshID != id::INVALID_ID;
}

bool
LoadEngineShaders(std::unique_ptr<u8[]>& shaders, u64& size)
{
    bool readEngineShaders{ ReadFileToByteBuffer(std::filesystem::path{ graphics::GetEngineShadersPath() }, shaders, size) };
    CreateDefaultShaders();
    CreateDefaultMaterial();
    return readEngineShaders;
}

bool
LoadEngineShaders(Array<std::unique_ptr<u8[]>>& shaders, u64& size)
{
    for (u32 i{0}; i < EngineShader::Count; ++i)
    {
        std::filesystem::path path{ graphics::GetEngineShaderPath((EngineShader::ID)i) };
        bool readShader{ ReadFileToByteBuffer(path, shaders[i], size) };
        if (!readShader) return false;
	}
    CreateDefaultShaders();
    CreateDefaultMaterial();
    return true;
}

bool
LoadEngineShader(std::unique_ptr<u8[]>& shader, EngineShader::ID shaderID)
{
    std::filesystem::path path{ graphics::GetEngineShaderPath((EngineShader::ID)shaderID) };
	u64 size{ 0 };
    bool readShader{ ReadFileToByteBuffer(path, shader, size) };
    if (!readShader) return false;
    return true;
}

bool
LoadDebugEngineShaders(Array<std::unique_ptr<u8[]>>& shaders, u64& size)
{
    for (u32 i{ 0 }; i < EngineDebugShader::Count; ++i)
    {
        std::filesystem::path path{ graphics::GetDebugEngineShaderPath((EngineDebugShader::ID)i) };
        bool readShader{ ReadFileToByteBuffer(path, shaders[i], size) };
        if (!readShader) return false;
    }
    return true;
}

bool
LoadDebugEngineShaders(std::unique_ptr<u8[]>& shaders, u64& size)
{
    bool readShaders{ ReadFileToByteBuffer(std::filesystem::path{ graphics::GetDebugEngineShadersPath() }, shaders, size) };
    return readShaders;
}

bool
LoadPhysicsDebugShaders(std::unique_ptr<u8[]>& shaders, u64& size)
{
    bool readShaders{ ReadFileToByteBuffer(std::filesystem::path{ shaders::physics::SHADERS_BIN_PATHS[0] }, shaders, size) };
    //for (u32 i{ 0 }; i < shaders::physics::DebugShaders::Count; ++i)
    //{
    //    std::filesystem::path path{ shaders::physics::SHADERS_BIN_PATHS[i] };
    //    bool readShader{ ReadFileToByteBuffer(path, shaders[i], size) };
    //    if (!readShader) return false;
    //}
    return readShaders;
}

void 
GeneratePrimitiveMeshAsset(PrimitiveMeshInfo info)
{
    MeshGroupData data{};
	GeneratePrimitiveMesh(info, data);
	SaveGeometry(data, editor::project::GetResourceDirectory() / "cube.geom");
}

bool
LoadBRDFLut(AssetHandle handle)
{
    return content::INVALID_HANDLE;
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
}

id_t 
CreateResourceFromAsset(std::filesystem::path path, AssetType::type assetType)
{
    AssetHandle handle{ assets::GetHandleFromImportedPath(path) };
    assert(content::IsValid(handle));
    if (!content::IsValid(handle)) handle = {};
    id_t existingID{ assets::GetResourceFromAsset(handle, assetType, false) };
    if (id::IsValid(existingID))
    {
        return existingID;
    }

    std::unique_ptr<u8[]> buffer;
    u64 size{ 0 };
    assert(std::filesystem::exists(path));
    content::ReadAssetFileNoVersion(path, buffer, size, assetType);
    assert(buffer.get());

    id_t resourceID{ content::CreateResourceFromBlob(buffer.get(), assetType) };
    assert(id::IsValid(resourceID));
    assets::PairAssetWithResource(handle, resourceID, assetType);
    return resourceID;
}

id_t 
CreateMaterial(graphics::MaterialInitInfo initInfo, AssetHandle handle)
{
    id_t matID{ content::CreateResourceFromBlob(&initInfo, content::AssetType::Material) };
    if(content::IsValid(handle)) assets::PairAssetWithResource(handle, matID, content::AssetType::Material);
    return matID;
}

void
ReadAssetFile(std::filesystem::path path, std::unique_ptr<u8[]>& dataOut, u64& sizeOut, AssetType::type type)
{
    //assert(type == GetAssetTypeFromExtension(path.extension().string()));

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
}

void
ReadAssetFileNoVersion(std::filesystem::path path, std::unique_ptr<u8[]>& dataOut, u64& sizeOut, [[maybe_unused]] AssetType::type type)
{
    //assert(type == GetAssetTypeFromExtension(path.extension().string()));

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
}

bool
RecompileDefaultShaders()
{
    shaders::ShaderFileInfo info{};
    info.File = "TestShader.hlsl";
    info.EntryPoint = "TestShaderVS";
    info.Type = shaders::ShaderType::Vertex;
    const char* shaderPath{ "..\\ExampleApp\\" };

    std::wstring defines[]{ L"ELEMENTS_TYPE=1", L"ELEMENTS_TYPE=3" };
    Vec<u32> keys{};
    keys.emplace_back((u32)content::ElementType::StaticNormal);
    keys.emplace_back((u32)content::ElementType::StaticNormalTexture);

    Vec<std::wstring> extraArgs{};
    Vec<std::unique_ptr<u8[]>> vertexShaders{};
    Vec<const u8*> vertexShaderPtrs{};
    for (u32 i{ 0 }; i < keys.size(); ++i)
    {
        extraArgs.clear();
        extraArgs.emplace_back(L"-D");
        extraArgs.emplace_back(defines[i]);
        vertexShaders.emplace_back(std::move(shaders::CompileShader(info, shaderPath, extraArgs)));
        if (!vertexShaders.back().get()) return false;
        vertexShaderPtrs.emplace_back(vertexShaders.back().get());
    }

    info.EntryPoint = "TestShaderPS";
    info.Type = shaders::ShaderType::Pixel;
    Vec<std::unique_ptr<u8[]>> pixelShaders{};

    extraArgs.clear();
    pixelShaders.emplace_back(std::move((shaders::CompileShader(info, shaderPath, extraArgs))));
    assert(pixelShaders.back().get());
    Vec<const u8*> psShaderPtrs{};

    extraArgs.clear();
    std::vector<std::vector<std::wstring>> psDefines{
        {L"TEXTURED_MTL=1"},
        {L"TEXTURED_MTL=1", L"ALPHA_TEST=1"},
        {L"TEXTURED_MTL=1", L"ALPHA_BLEND=1"},
    };
    Vec<u32> psKeys{};
    psKeys.emplace_back((u32)graphics::MaterialType::Opaque);
    psKeys.emplace_back((u32)graphics::MaterialType::AlphaTested);
    psKeys.emplace_back((u32)graphics::MaterialType::AlphaBlended);
    for (u32 i{ 0 }; i < psKeys.size(); ++i)
    {
        extraArgs.clear();
        for (auto& s : psDefines[i])
        {
            extraArgs.emplace_back(L"-D");
            extraArgs.emplace_back(s);
        }

        pixelShaders.emplace_back(std::move(shaders::CompileShader(info, shaderPath, extraArgs)));
        if (!pixelShaders.back().get()) return false;
        psShaderPtrs.emplace_back(pixelShaders.back().get());
    }

    const u8* untexturedPSPtr{ pixelShaders[0].get() };
    const u32 untexturedKey{ (u32)graphics::MaterialType::Opaque };
    content::UpdateShaderGroup(defaultVSID, vertexShaderPtrs.data(), (u32)vertexShaderPtrs.size(), keys.data());
    content::UpdateShaderGroup(defaultPSID, &untexturedPSPtr, 1, &untexturedKey);

    content::UpdateShaderGroup(defaultTexturedPSID, psShaderPtrs.data(), (u32)psShaderPtrs.size(), psKeys.data());
    return true;
}

}
