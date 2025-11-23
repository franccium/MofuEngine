#include "EditorContentManager.h"
#include "Editor/Project/Project.h"
#include "yaml-cpp/yaml.h"
#include <fstream>
#include "Editor/ContentBrowser.h"
#include "AssetImporter.h"
#include <ranges>
#include "Utilities/Logger.h"
#include <unordered_set>
#include "Content/ContentUtils.h"
#include "Editor/Material.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "Editor/SceneEditorView.h"
#include "Shaders/ContentProcessingShaders.h"
#include "D3D12EnvironmentMapProcessing.h"

namespace mofu::content::assets {
namespace {

std::unordered_map<AssetHandle, AssetPtr> assetRegistry{};

//TODO: check thread-safeness of this
std::array<Vec<std::pair<AssetHandle, id_t>>, AssetType::Count> assetResourcePairs{};

bool
SerializeRegistry()
{
	const std::filesystem::path& registryPath{ mofu::editor::project::GetAssetRegistryPath() };
	assert(std::filesystem::exists(registryPath) && registryPath.extension() == ".ar");

	YAML::Emitter out;
	{
		out << YAML::BeginMap;

		// write out each asset
		for (const auto& [handle, asset] : assetRegistry)
		{
			out << YAML::Key << handle.id;
			out << YAML::Value << YAML::BeginMap;

			out << YAML::Key << "Type" << YAML::Value << static_cast<u32>(asset->Type);
			out << YAML::Key << "OriginalFilePath" << YAML::Value << asset->OriginalFilePath.string();
			out << YAML::Key << "ImportedFilePath" << YAML::Value << asset->ImportedFilePath.string();

			out << YAML::EndMap;
		}

		out << YAML::EndMap;
	}

	std::ofstream outFile{ registryPath };
	outFile << out.c_str();

	return true;
}

bool
DeserializeRegistry()
{
	const std::filesystem::path& registryPath{ mofu::editor::project::GetAssetRegistryPath() };
	assert(std::filesystem::exists(registryPath) && registryPath.extension() == ".ar");

	YAML::Node data{ YAML::LoadFile(registryPath.string()) };
	assert(data.IsDefined());

	for (const auto& item : data) 
	{
		u64 handleId = item.first.as<u64>();
		const YAML::Node& assetNode = item.second;

		AssetType::type type = static_cast<AssetType::type>(assetNode["Type"].as<u32>());
		std::string origPath = assetNode["OriginalFilePath"].as<std::string>();
		std::string impPath = assetNode["ImportedFilePath"].as<std::string>();

		Asset* asset = new Asset{ type, origPath, impPath };
		RegisterAsset(asset, handleId);
	}

	return true;
}

//TODO: actual deletion
bool
DeserializeRegistryDeleteAssets()
{
	const std::filesystem::path& registryPath{ mofu::editor::project::GetAssetRegistryPath() };
	assert(std::filesystem::exists(registryPath) && registryPath.extension() == ".ar");

	YAML::Node data{ YAML::LoadFile(registryPath.string()) };
	assert(data.IsDefined());

	YAML::Node newData;

	for (const auto& item : data)
	{
		u64 handleId = item.first.as<u64>();
		const YAML::Node& assetNode = item.second;

		std::string origPath = assetNode["OriginalFilePath"].as<std::string>();

		if (origPath.find("Bistro") == std::string::npos) 
		{
			newData[handleId] = assetNode;

			AssetType::type type = static_cast<AssetType::type>(assetNode["Type"].as<u32>());
			std::string impPath = assetNode["ImportedFilePath"].as<std::string>();

			Asset* asset = new Asset{ type, origPath, impPath };
			RegisterAsset(asset, handleId);
		}
	}

	std::ofstream fout(registryPath);
	fout << newData;
	fout.close();

	return true;
}

void
RegisterAllAssetsInProject()
{
	std::filesystem::path assetBaseDirectory{ editor::project::GetAssetDirectory() };
	for (const auto& dirEntry : std::filesystem::recursive_directory_iterator{ assetBaseDirectory })
	{
		if (!dirEntry.is_regular_file()) continue;

		const std::filesystem::path& path{ dirEntry.path() };
		std::string extension{ path.extension().string() };

		if (IsEngineAssetExtension(extension)) continue;

		if (IsAllowedAssetExtension(extension) && !IsAssetAlreadyRegistered(path))
		{
			Asset* asset{ new Asset{ GetAssetTypeFromExtension(extension), path, {} } };
			RegisterAsset(asset);
		}
	}
}

//TODO: adhoc to make shaders visible in the content browser
void
RegisterShaders()
{
	std::filesystem::path shaderBaseDirectory{ editor::project::GetShaderDirectory() };
	for (const auto& dirEntry : std::filesystem::recursive_directory_iterator{ shaderBaseDirectory })
	{
		if (!dirEntry.is_regular_file()) continue;

		const std::filesystem::path& path{ dirEntry.path() };
		std::string extension{ path.extension().string() };
		if (extension != graphics::GetShaderFileExtension()) continue;

		if (!IsAssetAlreadyRegistered(path))
		{
			AssetHandle handle{ ImportAsset(path, {}) };
			AssetPtr asset{ GetAsset(handle) };
			PairAssetWithResource(handle, asset->AdditionalData, AssetType::Shader);
			PairAssetWithResource(handle, asset->AdditionalData2, AssetType::Shader);

			content::AssetHandle handle1{ content::assets::GetAssetFromResource(asset->AdditionalData, content::AssetType::Shader) };
			content::AssetHandle handle2{ content::assets::GetAssetFromResource(asset->AdditionalData2, content::AssetType::Shader) };
			assert(handle1 == handle2);
		}
	}
}

} // anonymous namespace

AmbientLightHandles 
GetAmbientLightHandles(AssetHandle skyboxHandle)
{
	AmbientLightHandles handles{};
	handles.SkyboxHandle = skyboxHandle;
	handles.DiffuseHandle = content::assets::GetIBLRelatedHandle(skyboxHandle);
	handles.SpecularHandle = content::assets::GetIBLRelatedHandle(handles.DiffuseHandle);
	handles.BrdfLutHandle = content::assets::GetIBLRelatedHandle(handles.SpecularHandle);
	return handles;
}

bool
IsAssetAlreadyRegistered(const std::filesystem::path& path)
{
	//FIXME: change in path outside of the project will not be reflected etc
	auto matchedAsset = std::ranges::find_if(assetRegistry, [&](const auto& item) {
			return item.second->OriginalFilePath == path;
		});
	return matchedAsset != assetRegistry.end();
}

bool
IsHandleValid(AssetHandle handle)
{
	return assetRegistry.find(handle) != assetRegistry.end();
}

// shouldn't be called too often, only needed for single item lookups in file browser
AssetHandle 
GetHandleFromPath(const std::filesystem::path& path)
{
	assert(IsAllowedAssetExtension(path.extension().string()));
	assert(!IsEngineAssetExtension(path.extension().string()));
	auto matchedAsset = std::ranges::find_if(assetRegistry, [&](const auto& item) {
		return item.second->OriginalFilePath == path;
		});
	return matchedAsset != assetRegistry.end() ? matchedAsset->first : INVALID_HANDLE;
}

AssetHandle 
GetHandleFromImportedPath(const std::filesystem::path& path)
{
	assert(IsAllowedAssetExtension(path.extension().string()));
	assert(IsEngineAssetExtension(path.extension().string()));
	auto matchedAsset = std::ranges::find_if(assetRegistry, [&](const auto& item) {
		return item.second->ImportedFilePath == path;
		});
	return matchedAsset != assetRegistry.end() ? matchedAsset->first : INVALID_HANDLE;
}

AssetPtr
GetAsset(AssetHandle handle)
{
	if (!IsHandleValid(handle)) return nullptr;

	else if (assetRegistry.find(handle) != assetRegistry.end())
	{
		return assetRegistry.at(handle);
	}

	assert(false);
	return nullptr;
}

// generates a new AssetHandle and puts the asset in the registry
AssetHandle
RegisterAsset(AssetPtr asset)
{
	AssetHandle handle{};
	assetRegistry.emplace(handle, asset);
	editor::AddRegisteredAsset(handle, asset);
	return handle;
}

AssetHandle
RegisterAsset(AssetPtr asset, u64 id)
{
	assert(!GetAsset(id));
	AssetHandle handle{ id };
	assetRegistry.emplace(handle, asset);
	editor::AddRegisteredAsset(handle, asset);
	return handle;
}

void 
DeregisterAsset(AssetHandle handle)
{
	log::Warn("DeregisterAsset called with an invalid handle");
	if (!IsValid(handle)) return;
	assetRegistry.erase(handle);
	editor::RemoveRegisteredAsset(handle);
}

void
DeleteAsset(AssetHandle handle)
{
	AssetPtr asset{ GetAsset(handle) };
	editor::DeleteRegisteredAsset(handle);
	if(asset) delete asset;
	assetRegistry.erase(handle);
}

void
PairAssetWithResource(AssetHandle handle, id_t resourceID, AssetType::type type)
{
	assert(id::IsValid(resourceID));

	//TODO: check thread-safeness of this
	assetResourcePairs[type].push_back({ handle, resourceID });
}

AssetHandle
GetAssetFromResource(id_t resourceID, AssetType::type type)
{
	if (!id::IsValid(resourceID)) return INVALID_HANDLE;

	auto set{ assetResourcePairs[type] };
	auto result = std::ranges::find_if(set.begin(), set.end(), [&](const auto& item) {
		return item.second == resourceID;
		});

	if (result != set.end())
	{
		AssetHandle handle{ result->first };
		return handle;
	}
	return INVALID_HANDLE;
}

// finds an existing resource for that asset; if none, creates one
id_t
GetResourceFromAsset(AssetHandle handle, AssetType::type type, bool createIfNotFound)
{
	//TODO: this could have multiple results
	auto set{ assetResourcePairs[type] };
	auto result = std::ranges::find_if(set.begin(), set.end(), [&](const auto& item) {
		return item.first == handle;
		});

	if (result != set.end())
	{
		id_t id{ result->second };
		return id;
	}

	if (createIfNotFound)
	{
		log::Warn("No existing resource for handle %u, creating one...", handle.id);
		return CreateResourceFromHandle(handle);
	}
	return id::INVALID_ID;
}

Vec<id_t>
GetResourcesFromAsset(AssetHandle handle, AssetType::type type)
{
	//TODO: this could have multiple results
	assert(false);
	return {};
}

id_t 
CreateResourceFromHandle(AssetHandle handle)
{
	AssetPtr asset{ GetAsset(handle) };
	assert(asset);
	const AssetType::type type{ asset->Type };
	id_t existingResource{ GetResourceFromAsset(handle, type, false) };
	if (id::IsValid(existingResource)) return existingResource;

	if (!std::filesystem::exists(asset->ImportedFilePath))
	{
		log::Error("CreateResourceFromHandle: Asset not imported");
		assert(std::filesystem::exists(asset->OriginalFilePath));
		ImportAsset(handle);
	}

	std::unique_ptr<u8[]> buffer{};
	u64 size{};
	content::ReadAssetFileNoVersion(asset->ImportedFilePath, buffer, size, type);
	assert(buffer.get());

	id_t resourceID{ content::CreateResourceFromBlob(buffer.get(), type) };
	PairAssetWithResource(handle, resourceID, type);
	return resourceID;
}

void
ImportAllNotImported(AssetType::type type)
{
	// for now only textures
	for (const auto& [handle, asset] : assetRegistry)
	{
		if (std::filesystem::exists(asset->ImportedFilePath)) continue;

		// import
		if (asset->Type == type)
		{
			ImportAsset(handle);
		}
	}
}

// TODO: this is useless, only need it to register all materials
void
RegisterAllAssetsOfType(AssetType::type type)
{
	assert(type == AssetType::Material);

	Vec<std::string> files{};
	ListFilesByExtension(EXTENSION_FOR_ENGINE_ASSET[type], editor::project::GetResourceDirectory(), files);
	for (const auto& assetPath : files)
	{
		if (IsValid(GetHandleFromImportedPath(assetPath))) continue;
		
		editor::material::CreateMaterialAsset(assetPath);

		//std::unique_ptr<u8[]> buffer{};
		//u64 size{};
		//content::ReadAssetFileNoVersion(assetPath, buffer, size, type);

		//id_t resourceID{ content::CreateResourceFromBlob(buffer.get(), type) };
		//PairAssetWithResource(AssetHandle{}, resourceID, type);
	}
}

void 
LoadEditorAssets()
{
	AssetPtr mat{ GetAsset(DEFAULT_MATERIAL_UNTEXTURED_HANDLE) };
	PairAssetWithResource(DEFAULT_MATERIAL_UNTEXTURED_HANDLE, content::GetDefaultMaterial(), content::AssetType::Material);
	AssetPtr mesh{ GetAsset(DEFAULT_MESH_HANDLE) };
	content::LoadEngineMeshes(mesh->ImportedFilePath);
	//AssetPtr mesh{ GetAsset(DEFAULT_BRDF_LUT_HANDLE) };
	//content::LoadBRDFLut(DEFAULT_BRDF_LUT_HANDLE);
}

void 
SaveAssetRegistry()
{
	SerializeRegistry();
}

// loads the registry from file
void
InitializeAssetRegistry()
{
	shaders::content::Initialize();
	texture::InitializeEnvironmentProcessing();

	editor::InitializeAssetBrowserGUI();

	DeserializeRegistry();
	//DeserializeRegistryDeleteAssets();

	RegisterAllAssetsInProject();
	RegisterShaders();

	LoadEditorAssets();
}

// saves the registry to a file
void
ShutdownAssetRegistry()
{
	//texture::ShutdownEnvironmentProcessing();
	shaders::content::Shutdown();

	SerializeRegistry();

	for (auto& [handle, asset] : assetRegistry)
	{
		delete asset;
	}
}

void 
GetTextureIconData(const std::filesystem::path& path, u64& outIconSize, std::unique_ptr<u8[]>& iconBuffer)
{
	assert(std::filesystem::exists(path) && path.extension().string() == ASSET_METADATA_EXTENSION);

	std::ifstream file{ path, std::ios::in | std::ios::binary };
	if (!file)
	{
		log::Error("Failed to open file for reading: %s", path.string().c_str());
		return;
	}

	u32 iconSize{};
	file.read(reinterpret_cast<char*>(&iconSize), sizeof(iconSize));
	if (iconSize == 0)
	{
		log::Warn("Can't find icon data for texture: %s", path.string().c_str());
		return;
	}
	outIconSize = iconSize;
	iconBuffer = std::make_unique<u8[]>(outIconSize);
	file.read(reinterpret_cast<char*>(iconBuffer.get()), iconSize);
}

AssetHandle 
GetIBLRelatedHandle(AssetHandle cubemapHandle)
{
	if (!IsValid(cubemapHandle))
	{
		log::Warn("GetIBLRelatedHandle called with an invalid handle");
		return INVALID_HANDLE;
	}
	AssetPtr asset{ GetAsset(cubemapHandle) };
	if (!asset)
	{
		return INVALID_HANDLE;
	}
	std::filesystem::path metadataPath{ asset->GetMetadataPath() };
	assert(std::filesystem::exists(metadataPath) && metadataPath.extension().string() == ASSET_METADATA_EXTENSION);
	if (std::filesystem::exists(metadataPath))
	{
		std::unique_ptr<u8[]> buffer{};
		u64 bufferSize;
		ReadFileToByteBuffer(metadataPath, buffer, bufferSize);
		assert(buffer.get() && bufferSize);

		util::BlobStreamReader reader{ buffer.get() };
		const u32 iconSize{ reader.Read<u32>() };
		reader.Skip(iconSize);
		reader.Skip(reader.Read<u32>()); // name
		reader.Skip(sizeof(u32) * 6 + sizeof(u8) * 4 + sizeof(u32) * 6);
		u64 iblPairID{ reader.Read<u64>() };
		asset->AdditionalData2 = iblPairID;
		return AssetHandle{ iblPairID };
	}
	log::Error("GetIBLRelatedHandle: metadata not found");
	return INVALID_HANDLE;
}

void 
ParseMetadata(AssetPtr asset)
{
	//TODO:
	switch (asset->Type)
	{
	case content::AssetType::Texture:
	{
#if ASSET_ICONS_ENABLED
		std::filesystem::path metadataPath{ asset->GetMetadataPath() };
		if (std::filesystem::exists(metadataPath) && !id::IsValid(asset->AdditionalData)) //FIXME: 0 is a valid id
		{
			std::unique_ptr<u8[]> iconBuffer{};
			u64 iconSize{};
			content::assets::GetTextureIconData(metadataPath, iconSize, iconBuffer);
			if (iconSize != 0)
			{
				id_t iconId{ graphics::ui::AddIcon(iconBuffer.get()) };
				asset->AdditionalData = iconId;
			}
		}
#else
		asset->AdditionalData = U32_INVALID_ID;
#endif
		break;
	}
	default:
		break;
	}
}

void
GetTextureMetadata(const std::filesystem::path& path, u64& outTextureSize, std::unique_ptr<u8[]>& textureBuffer, 
	u64& outIconSize, std::unique_ptr<u8[]>& iconBuffer)
{
	assert(std::filesystem::exists(path) && path.extension().string().c_str() == ASSET_METADATA_EXTENSION);

	std::ifstream file{ path, std::ios::in | std::ios::binary };
	if (!file)
	{
		log::Error("Failed to open file for reading: %s", path.string().c_str());
		return;
	}

	u32 iconSize{};
	file.read(reinterpret_cast<char*>(&iconSize), sizeof(iconSize));
	assert(iconSize != 0);
	if (iconSize == 0)
	{
		log::Warn("Can't find icon data for texture: %s", path.string().c_str());
	}
	outIconSize = iconSize;
	iconBuffer = std::make_unique<u8[]>(outIconSize);
	file.read(reinterpret_cast<char*>(iconBuffer.get()), iconSize);

	outTextureSize = std::filesystem::file_size(path) - iconSize - sizeof(iconSize);
	assert(outTextureSize != 0);

	textureBuffer = std::make_unique<u8[]>(outTextureSize);
	file.read(reinterpret_cast<char*>(textureBuffer.get()), outTextureSize);
}

void 
GetGeometryRelatedTextures(AssetHandle geometryHandle, Vec<AssetHandle>& outHandles)
{
	assert(IsValid(geometryHandle));
	AssetPtr geometryAsset{ GetAsset(geometryHandle) };
	assert(std::filesystem::exists(geometryAsset->GetMetadataPath()));
	u64 size{};
	std::unique_ptr<u8[]> buffer{};
	ReadFileToByteBuffer(geometryAsset->GetMetadataPath(), buffer, size);

	util::BlobStreamReader reader{ buffer.get() };
	while (reader.Offset() < size)
	{
		outHandles.emplace_back(reader.Read<u64>());
	}
	assert(reader.Offset() == size);
}

void
LoadMeshAsset(AssetHandle asset, ecs::Entity entity, ecs::component::RenderMesh& mesh, ecs::component::RenderMaterial& material)
{
	if (id::IsValid(mesh.MeshID))
	{
		//TODO: handle this?
	}

	id_t meshID{ CreateResourceFromHandle(asset) };
	content::UploadedGeometryInfo uploadedGeometryInfo{ content::GetLastUploadedGeometryInfo() };
	u32 submeshCount{ uploadedGeometryInfo.SubmeshCount };

	assert(id::IsValid(material.MaterialID));
	if (!id::IsValid(material.MaterialID))
	{
		material.MaterialCount = 1;
		material.MaterialID = content::GetDefaultMaterial();
		material.MaterialAsset = DEFAULT_MATERIAL_UNTEXTURED_HANDLE;
	}
	// root
	mesh.MeshID = uploadedGeometryInfo.GeometryContentID;

	if (submeshCount > 1)
	{
		// have to create some entities
		struct RenderableEntitySpawnContext
		{
			ecs::Entity	entity;
			ecs::component::RenderMesh Mesh;
			ecs::component::RenderMaterial Material;
		};
		Vec<RenderableEntitySpawnContext> spawnedEntities(submeshCount);
		std::string entityName{ "this todo" };
		//strcpy(name.Name, filename.data());

		spawnedEntities[0] = { entity, mesh, material };

		ecs::component::Child child{ {}, entity };
		ecs::component::LocalTransform lt{};
		ecs::component::WorldTransform wt{};
		ecs::component::RenderMesh mesh{};
		ecs::component::RenderMaterial mat{};
		for (u32 i{ 1 }; i < submeshCount; ++i)
		{
			id_t meshId{ uploadedGeometryInfo.SubmeshGpuIDs[i] };
			mesh.MeshID = meshId;
			mat.MaterialID = material.MaterialID;
			mat.MaterialCount = 1;
			//snprintf(name.Name, ecs::component::NAME_LENGTH, "child %u", i);

			ecs::EntityData& e{ ecs::scene::SpawnEntity<ecs::component::LocalTransform, ecs::component::WorldTransform,
				ecs::component::RenderMesh, ecs::component::RenderMaterial, ecs::component::Child>(
					lt, wt, mesh, mat, child) };
			spawnedEntities[i] = { e.id, mesh, material };
		}

		for (auto& c : spawnedEntities)
		{
			ecs::component::RenderMesh& mesh{ ecs::scene::GetComponent<ecs::component::RenderMesh>(c.entity) };
			mesh.RenderItemID = graphics::AddRenderItem(c.entity, c.Mesh.MeshID, c.Material.MaterialCount, c.Material.MaterialID);
			if(c.entity != entity) 
				editor::AddEntityToSceneView(c.entity);
		}
	}
	else
	{
		id_t oldID{ mesh.RenderItemID };
		mesh.RenderItemID = graphics::AddRenderItem(entity, mesh.MeshID, material.MaterialCount, material.MaterialID);
		//graphics::UpdateRenderItemData(oldID, mesh.RenderItemID);
	}
}


}
