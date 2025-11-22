#include "AssetImporter.h"
#include <functional>
#include <array>
#include <unordered_map>
#include "Utilities/Logger.h"
#include "External/ufbx/ufbx.h"
#include "ContentUtils.h"
#include "Content/TextureImport.h"
#include "Editor/AssetPacking.h"
#include "Editor/TextureView.h"
#include "Editor/ImporterView.h"
#include "Editor/MaterialEditor.h"
#include "Editor/Project/Project.h"
#include "EditorContentManager.h"
#include "PhysicsImporter.h"
#include "Content/ShaderCompilation.h"

namespace mofu::content {
namespace {

std::filesystem::path 
BuildResourcePath(std::string_view assetFileName, const char* extension)
{
	std::filesystem::path p{ editor::project::GetResourceDirectory() / assetFileName };
	p.replace_extension(extension);
	return p;
}

const std::filesystem::path
ImportUnknown([[maybe_unused]] std::filesystem::path path, [[maybe_unused]] AssetPtr asset, [[maybe_unused]] const std::filesystem::path& importedPath)
{
	assert(false);
	return {};
}

AssetHandle
CreateBRDFLut(const texture::TextureData& data, std::filesystem::path targetPath)
{
	texture::TextureData lutData{};
	lutData.ImportSettings = data.ImportSettings;
	lutData.ImportSettings.Compress = false;
	lutData.ImportSettings.MipLevels = 1;

	texture::ComputeBRDFIntegrationLUT(&lutData);

	if (lutData.Info.ImportError != texture::ImportError::Succeeded)
	{
		log::Error("BRDF LUT creation error: %s", texture::TEXTURE_IMPORT_ERROR_STRING[lutData.Info.ImportError]);
		return INVALID_HANDLE;
	}

	PackTextureForEngine(lutData, targetPath);
	Asset* asset = new Asset{ content::AssetType::Texture, targetPath, targetPath };
	asset->AdditionalData2 = AssetFlag::Flag::IsBRDFLut;
	std::filesystem::path metadataPath{ targetPath.replace_extension(ASSET_METADATA_EXTENSION) };
	PackTextureForEditor(lutData, metadataPath);

	AssetHandle handle{ assets::RegisterAsset(asset) };

	std::unique_ptr<u8[]> iconBuffer{};
	u64 iconSize{};
	content::assets::GetTextureIconData(metadataPath, iconSize, iconBuffer);
	if (iconSize != 0)
	{
		id_t iconId{ graphics::ui::AddIcon(iconBuffer.get()) };
		asset->AdditionalData = iconId;
	}

	delete[] lutData.SubresourceData;
	if (lutData.Icon) delete[] lutData.Icon;
	return handle;
}

void
CreateTextureAssetIcon(AssetPtr asset)
{
	//TODO: a generic metadata file for this stuff?
	std::filesystem::path iconPath{ asset->ImportedFilePath };
	iconPath.replace_extension(ASSET_METADATA_EXTENSION);
	std::unique_ptr<u8[]> buffer;
	u64 size{ 0 };
	assert(std::filesystem::exists(iconPath));
	content::ReadAssetFileNoVersion(std::filesystem::path(iconPath), buffer, size, AssetType::Texture);
	assert(buffer.get());

	id_t assetID{ graphics::ui::AddIcon(buffer.get()) };
	assert(id::IsValid(assetID));
	asset->AdditionalData = assetID;
}

const std::filesystem::path
ImportTexture(std::filesystem::path path, AssetPtr asset, const std::filesystem::path& importedPath)
{
	//assert(std::filesystem::exists(path));
	texture::TextureData data{};
	data.ImportSettings = editor::assets::GetTextureImportSettings();

	//data.ImportSettings.Files = path.string(); //TODO: char* 
	//data.ImportSettings.FileCount = 1;
	//TODO: handling missing textures this way could be problematic with remembering what asset it was supposed to actually import?
	if (data.ImportSettings.Files.empty() || data.ImportSettings.FileCount == 0)
	{
		data.ImportSettings.Files.append(path.string());
		data.ImportSettings.FileCount = 1;
	}

	texture::Import(&data);
	if (data.Info.ImportError != texture::ImportError::Succeeded)
	{
		log::Error("Texture import error: %s", texture::TEXTURE_IMPORT_ERROR_STRING[data.Info.ImportError]);
		return {};
	}

	const bool isCubemap{ (data.Info.Flags & TextureFlags::IsCubeMap) != 0 };
	auto path_str = importedPath.string();
	std::filesystem::path texturePath{ path_str };
	const std::string originalFilename{ path.stem().string() };
	const bool isReimporting{ !importedPath.empty() };
	//NOTE: should be scratched, as a new handle might be registered if called using (path, importedPath)
	const AssetHandle existingHandle{ isReimporting ? assets::GetHandleFromImportedPath(importedPath) : INVALID_HANDLE };

	if (!isReimporting)
	{
		texturePath = !isCubemap ? editor::project::GetTextureDirectory() : editor::project::GetResourceDirectory() / "Cubemaps";
		texturePath.append(originalFilename + "_sky" + ".tex");
	}
	else if (isCubemap)
	{
		assert(IsValid(existingHandle));
		// if we are reimporting cubemap without prefiltering, preserve any existing connections to prefiltered cubemaps
		data.Info.IBLPair = assets::GetIBLRelatedHandle(existingHandle);
	}

	if (isCubemap && data.ImportSettings.PrefilterCubemap)
	{
		auto cloneData{ [](const texture::TextureData& srcData) {
			texture::TextureData newData;
			newData.SubresourceData = new u8[srcData.SubresourceSize];
			memcpy(newData.SubresourceData, srcData.SubresourceData, srcData.SubresourceSize);
			newData.SubresourceSize = srcData.SubresourceSize;
			newData.Icon = new u8[srcData.IconSize];
			memcpy(newData.Icon, srcData.Icon, srcData.IconSize);
			newData.IconSize = srcData.IconSize;
			newData.Info = srcData.Info;
			newData.ImportSettings = srcData.ImportSettings;
			return newData;
		} };

		texture::TextureData diffuseData{ cloneData(data) };
		texture::PrefilterDiffuseIBL(&diffuseData);
		if (diffuseData.Info.ImportError != texture::ImportError::Succeeded)
		{
			log::Error("Texture import error when creating Diffuse IBL: %s", texture::TEXTURE_IMPORT_ERROR_STRING[diffuseData.Info.ImportError]);
			return {};
		}

		texture::TextureData specularData{ cloneData(data) };
		specularData.ImportSettings.IsSpecularCubemap = true;
		texture::PrefilterSpecularIBL(&specularData);
		if (specularData.Info.ImportError != texture::ImportError::Succeeded)
		{
			log::Error("Texture import error when creating Specular IBL: %s", texture::TEXTURE_IMPORT_ERROR_STRING[specularData.Info.ImportError]);
			return {};
		}

		if (isReimporting)
		{
			assets::AmbientLightHandles handles{ assets::GetAmbientLightHandles(existingHandle) };
			// delete the assets from registry to avoid fetching old versions by path
			if (id::IsValid(handles.DiffuseHandle))
			{
				assets::DeregisterAsset(handles.DiffuseHandle);
				assets::DeregisterAsset(handles.SpecularHandle);
				assets::DeregisterAsset(handles.BrdfLutHandle);
			}
		}

		std::filesystem::path diffusePath{ texturePath };
		diffusePath.replace_filename(originalFilename + "_diff" + ".tex");
		Asset* diffuseAsset = new Asset{ content::AssetType::Texture, path, diffusePath };
		diffuseAsset->RelatedCount = diffuseData.ImportSettings.FileCount;
		const content::AssetHandle diffuseHandle{ assets::RegisterAsset(diffuseAsset) };

		std::filesystem::path specPath{ diffusePath };
		specPath.replace_filename(originalFilename + "_spec" + ".tex");
		Asset* specularAsset = new Asset{ content::AssetType::Texture, path, specPath };
		specularAsset->RelatedCount = diffuseData.ImportSettings.FileCount;
		const content::AssetHandle specularHandle{ assets::RegisterAsset(specularAsset) };
		data.Info.IBLPair = diffuseHandle;
		asset->AdditionalData2 = diffuseHandle;
		diffuseData.Info.IBLPair = specularHandle;
		diffuseAsset->AdditionalData2 = specularHandle;


		PackTextureForEngine(diffuseData, diffusePath);
		std::filesystem::path metadataPath{ diffusePath.replace_extension(ASSET_METADATA_EXTENSION) };
		PackTextureForEditor(diffuseData, metadataPath);

		std::filesystem::path brdfLutPath{ diffusePath.replace_filename(originalFilename + "_brdflut.tex") };
		const AssetHandle brdfLUTHandle{ CreateBRDFLut(diffuseData, brdfLutPath) };
		assert(IsValid(brdfLUTHandle));
		if (IsValid(brdfLUTHandle))
		{
			specularAsset->AdditionalData2 = brdfLUTHandle;
			specularData.Info.IBLPair = brdfLUTHandle;
		}

		PackTextureForEngine(specularData, specPath);
		std::filesystem::path specMetadataPath{ specPath.replace_extension(ASSET_METADATA_EXTENSION) };
		PackTextureForEditor(specularData, specMetadataPath);

		/*AssetHandle diffuseHandle{ asset->AdditionalData2 };
		AssetHandle specularHandle{ load from diffuse asset texture data };
		AssetHandle brdfLUTHandle{ diffuseAsset->AdditionalData2 };*/

		delete[] diffuseData.SubresourceData;
		delete[] specularData.SubresourceData;
		delete[] diffuseData.Icon;
		delete[] specularData.Icon;
	}

	
	asset->ImportedFilePath = texturePath;
	asset->RelatedCount = data.ImportSettings.FileCount;
	PackTextureForEngine(data, texturePath);
	std::filesystem::path metadataPath{ texturePath.replace_extension(ASSET_METADATA_EXTENSION) };
	PackTextureForEditor(data, metadataPath);

	std::unique_ptr<u8[]> iconBuffer{};
	u64 iconSize{};
	content::assets::GetTextureIconData(metadataPath, iconSize, iconBuffer);
	if (iconSize != 0)
	{
		id_t iconId{ graphics::ui::AddIcon(iconBuffer.get()) };
		asset->AdditionalData = iconId;
	}

	delete[] data.SubresourceData;
	delete[] data.Icon;

	return asset->ImportedFilePath;
}

Color 
ColorFromMaterial(const ufbx_material_map& matMap) {
	if (matMap.value_components == 1) 
	{
		f32 r = (f32)matMap.value_real;
		return { r, r, r };
	}
	else if (matMap.value_components == 3)
	{
		f32 r = (f32)matMap.value_vec3.x;
		f32 g = (f32)matMap.value_vec3.y;
		f32 b = (f32)matMap.value_vec3.z;
		return { r, g, b };
	}
	f32 r = (f32)matMap.value_vec4.x;
	f32 g = (f32)matMap.value_vec4.y;
	f32 b = (f32)matMap.value_vec4.z;
	f32 a = (f32)matMap.value_vec4.z;
	return Color(r, g, b, a);
}

Color 
ColorFromMaterial(const ufbx_material_map& matMap, const ufbx_material_map& factorMap) {
	Color color = ColorFromMaterial(matMap);
	if (factorMap.has_value)
	{
		f32 factor = f32(factorMap.value_real);
		color.r *= factor;
		color.g *= factor;
		color.b *= factor;
	}
	return color;
}

const ufbx_texture* 
GetFileTexture(const ufbx_texture* texPtr)
{
	if (!texPtr) return nullptr;
	for (const ufbx_texture* tex : texPtr->file_textures)
	{
		if (tex->file_index != UFBX_NO_INDEX) return tex;
	}
	return nullptr;
}


void
ImportImageFromBytes(const u8* const bytes, u64 size, const char* fileExtension, AssetPtr asset, const std::filesystem::path& targetPath)
{
	texture::TextureData data{};
	data.ImportSettings.IsByteArray = true;
	data.ImportSettings.ImageBytesSize = (u32)size;
	data.ImportSettings.ImageBytes = bytes;
	data.ImportSettings.FileExtension = fileExtension;

	data.ImportSettings.Compress = true;
	texture::Import(&data);
	if (data.Info.ImportError != texture::ImportError::Succeeded)
	{
		log::Error("Texture import error: %s", texture::TEXTURE_IMPORT_ERROR_STRING[data.Info.ImportError]);
		return;
	}
	assert(data.SubresourceSize && data.SubresourceData);

	std::filesystem::path texturePath{ targetPath };
	texturePath.replace_extension(".tex");
	asset->ImportedFilePath = texturePath;
	asset->RelatedCount = 1;
	PackTextureForEngine(data, texturePath);

	std::filesystem::path metadataPath{ texturePath.replace_extension(ASSET_METADATA_EXTENSION) };
	PackTextureForEditor(data, metadataPath);

	std::unique_ptr<u8[]> iconBuffer{};
	u64 iconSize{};
	content::assets::GetTextureIconData(metadataPath, iconSize, iconBuffer);
	if (iconSize != 0)
	{
		id_t iconId{ graphics::ui::AddIcon(iconBuffer.get()) };
		asset->AdditionalData = iconId;
	}
	delete[] data.SubresourceData;
	delete[] data.Icon;
}

void
FindAllTextureFiles(FBXImportState* const state)
{
	std::filesystem::path startTextureScanPath{ state->ModelSourcePath };
	if (state->ModelSourcePath.stem().string() == "source")
	{
		startTextureScanPath = state->ModelSourcePath.parent_path();
	}
	Vec<std::string> textureFiles{};
	std::string texExtension{};
	//NOTE: this assumes that all textures are in one folder; most likely "/textures" or "/Textures" but it will find 
	// and pick the first one with a valid image
	for (const auto& entry : std::filesystem::recursive_directory_iterator{ startTextureScanPath })
	{
		if (entry.is_regular_file())
		{
			std::string ext{ entry.path().extension().string() };
			if (IsAllowedTextureExtension(ext))
			{
				startTextureScanPath = entry.path().parent_path();
				texExtension = ext;
				break;
			}
		}
	}

	ListFilesByExtensionRec(texExtension.c_str(), startTextureScanPath, textureFiles);
	if (!textureFiles.empty())
	{
		log::Info("Found %u texture files (%s)", (u32)textureFiles.size(), texExtension);
	}
	std::filesystem::path textureResourceBasePath{ state->ModelResourcePath };
	textureResourceBasePath.append("Textures");
	std::filesystem::create_directory(textureResourceBasePath);

	const u32 textureCount{ (u32)textureFiles.size() };
	state->AllTextureHandles.resize(textureCount);
	for (u32 i{0}; i < textureCount; ++i)
	{
		const std::filesystem::path texturePath{ textureFiles[i] };
		std::filesystem::path resourcePath{ textureResourceBasePath / texturePath.stem() };
		resourcePath.replace_extension(".tex");
		state->AllTextureHandles[i] = ImportAsset(texturePath, resourcePath);
	}
}

void
ImportImages(const ufbx_scene* fbxScene, const std::string_view basePath, FBXImportState* const state)
{
	state->Textures.resize(fbxScene->texture_files.count);
	state->SourceImages.resize(fbxScene->texture_files.count);
	state->ImageFiles.resize(fbxScene->texture_files.count);

	if (state->ImportSettings.TexturesFromImportedPath)
	{
		std::filesystem::path seekInPath{ state->ModelResourcePath / state->ImportSettings.TextureDirectory };
		if(!std::filesystem::exists(seekInPath))
		{
			log::Error("FBX Import: Could not find texture directory at path [%s]", seekInPath.string().data());
			state->Errors |= FBXImportState::ErrorFlags::InvalidTexturePath;
			return;
		}
		std::string textureBasePath{ seekInPath.append("").string() };

		for (u32 textureIdx{ 0 }; textureIdx < fbxScene->texture_files.count; ++textureIdx)
		{
			const ufbx_texture_file& fbxTexFile{ fbxScene->texture_files[textureIdx] };

			std::filesystem::path texturePath{ fbxTexFile.filename.data };
			std::string textureFilename{ texturePath.stem().string() };
			
			//TODO: look for textures

			std::string resourcePath{ textureBasePath + textureFilename + ".tex" };
			assert(std::filesystem::exists(resourcePath));
			if (!std::filesystem::exists(resourcePath))
			{
				log::Error("FBX Import: Could not find texture file [%s]");
			}
			state->Textures[textureIdx] = {};
			state->SourceImages[textureIdx] = textureIdx; // TODO: not sure if this is correct
			state->ImageFiles[textureIdx] = resourcePath;
		}
		return;
	}

	if (state->ImportSettings.FindAllTextureFiles)
	{
		FindAllTextureFiles(state);
	}

	if (state->ImportSettings.ImportEmbeddedTextures)
	{
		for (u32 textureIdx{ 0 }; textureIdx < fbxScene->texture_files.count; ++textureIdx)
		{
			const ufbx_texture_file& fbxTexFile{ fbxScene->texture_files[textureIdx] };

			std::filesystem::path texturePath{ fbxTexFile.filename.data };
			if (texturePath.is_absolute())
			{
				// these are weird so just try to find them in the source folder
				texturePath = texturePath.filename();
				std::filesystem::path startTextureScanPath{ state->ModelSourcePath };
				if (state->ModelSourcePath.stem().string() == "source")
				{
					startTextureScanPath = state->ModelSourcePath.parent_path();
				}
				if (std::filesystem::exists(startTextureScanPath / "textures"))
					texturePath = startTextureScanPath / "textures" / texturePath.filename();
				else if (std::filesystem::exists(startTextureScanPath / "Textures"))
					texturePath = startTextureScanPath / "Textures" / texturePath.filename();
			}
			if (!basePath.empty())
			{
				texturePath = texturePath.concat(basePath);
			}

			assert(fbxTexFile.content.size < UINT_MAX);
			u32 contentSize{ (u32)fbxTexFile.content.size };

			// get image data as a byte array
			std::unique_ptr<u8[]> data;
			if (contentSize > 0)
			{
				data = std::make_unique<u8[]>(contentSize);
				memcpy(data.get(), fbxTexFile.content.data, contentSize);
			}
			else
			{
				u64 outSize{ 0 };
				if (!std::filesystem::exists(texturePath))
				{
					state->Errors |= FBXImportState::ErrorFlags::InvalidTexturePath;
				}
				else if (std::filesystem::file_size(texturePath) != 0)
				{
					content::ReadFileToByteBuffer(texturePath, data, outSize);
					assert(outSize < UINT_MAX);
					contentSize = (u32)outSize;
				}
				if (contentSize == 0)
				{
					log::Warn("FBX Import: Image at path [%s] had no data", texturePath.string().data());
					state->Textures[textureIdx] = {};
					state->SourceImages[textureIdx] = {};
					state->ImageFiles[textureIdx] = {};
				}
			}

			if (contentSize > 0)
			{
				std::filesystem::path textureImagePath{ editor::project::GetAssetDirectory() / "ImportedScenes" / state->Name / "Textures" };
				std::filesystem::path resourcePath{ state->ModelResourcePath / "Textures" };
				if (!std::filesystem::exists(textureImagePath))
				{
					if (!std::filesystem::exists(textureImagePath.parent_path()))
						std::filesystem::create_directory(textureImagePath.parent_path());
					std::filesystem::create_directory(textureImagePath);
				}
				if (!std::filesystem::exists(resourcePath))
				{
					std::filesystem::create_directory(resourcePath);
				}
				//char name[16];
				//sprintf_s(name, "%u%s\0", textureIdx, texturePath.extension().string().data());
				textureImagePath /= texturePath.filename();
				resourcePath /= texturePath.filename();
				resourcePath.replace_extension(".tex");

				Asset* asset = new Asset{ AssetType::Texture, textureImagePath, resourcePath };
				ImportImageFromBytes(data.get(), contentSize, textureImagePath.extension().string().data(), asset, resourcePath);
				state->Textures[textureIdx] = {};
				state->SourceImages[textureIdx] = textureIdx; // TODO: not sure if this is correct
				state->ImageFiles[textureIdx] = resourcePath.string();

				assets::RegisterAsset(asset);
			}
		}
	}
}

void
GetTextureForMaterial(editor::material::TextureUsage::Usage textureUsage, editor::material::EditorMaterial& material, u32 fileIndex, FBXImportState* const state)
{
	//TODO: reuse already loaded

	if (!(state->ImportSettings.ImportEmbeddedTextures || state->ImportSettings.TexturesFromImportedPath)) return;
	u32 sourceImageIndex{ state->SourceImages[fileIndex] };
	if (!state->ImageFiles[sourceImageIndex].empty())
	{
		material.TextureIDs[textureUsage] = content::CreateResourceFromAsset(state->ImageFiles[sourceImageIndex], content::AssetType::Texture);
	}
	else
	{
		log::Warn("Couldn't find texture for %u, idx: %u %u", textureUsage, fileIndex, sourceImageIndex);
		state->Errors |= FBXImportState::ErrorFlags::MaterialTextureNotFound;
		material.TextureIDs[textureUsage] = id::INVALID_ID;
	}
}

void
ImportFBXMaterials(ufbx_scene* scene, FBXImportState* const state)
{
	for (ufbx_material* mat : scene->materials)
	{
		editor::material::EditorMaterial material{};
		graphics::MaterialSurface& surface{ material.Surface };
		material.Name = mat->name.data;
		using TexUse = editor::material::TextureUsage;

		material.Type = graphics::MaterialType::Opaque;

		if (mat->pbr.base_color.has_value)
		{
			Color baseColor{ ColorFromMaterial(mat->pbr.base_color).LinearToSRGB() };
			surface.BaseColor = { baseColor.r, baseColor.g,baseColor.b, baseColor.a };
		}

		const ufbx_texture* baseTexture{ GetFileTexture(mat->pbr.base_color.texture) };
		if (baseTexture)
		{
			bool wrap = (baseTexture->wrap_u | baseTexture->wrap_v) == UFBX_WRAP_REPEAT;

			// TODO: alpha
			if(wrap) material.Flags |= graphics::MaterialFlags::Flags::TextureRepeat;
			GetTextureForMaterial(TexUse::BaseColor, material, mat->pbr.base_color.texture->file_index, state);
		}

		//if (mat->features.pbr.enabled)
		{
			if (mat->pbr.metalness.has_value)
			{
				surface.Metallic = (f32)mat->pbr.metalness.value_real;
			}

			if (mat->pbr.roughness.has_value)
			{
				surface.Roughness = (f32)mat->pbr.roughness.value_real;
			}

			const ufbx_texture* metalnessTexture{ GetFileTexture(mat->pbr.metalness.texture) };
			if (metalnessTexture)
			{
				GetTextureForMaterial(TexUse::MetallicRoughness, material, mat->pbr.metalness.texture->file_index, state);
			}

			const ufbx_texture* roughnessTexture{ GetFileTexture(mat->pbr.roughness.texture) };
			if (roughnessTexture)
			{
				GetTextureForMaterial(TexUse::MetallicRoughness, material, mat->pbr.roughness.texture->file_index, state);
			}
		}

		const ufbx_texture* normalTexture{ GetFileTexture(mat->pbr.normal_map.texture) };
		if (normalTexture)
		{
			GetTextureForMaterial(TexUse::Normal, material, mat->pbr.normal_map.texture->file_index, state);
		}

		if (mat->pbr.emission_color.has_value)
		{
			Color emissionColor{ ColorFromMaterial(mat->pbr.emission_color).LinearToSRGB() };
			surface.Emissive = { emissionColor.r, emissionColor.g, emissionColor.b };
			surface.EmissiveIntensity = (f32)mat->pbr.emission_factor.value_real;
		}

		const ufbx_texture* emissionTexture{ GetFileTexture(mat->pbr.emission_color.texture) };
		if (emissionTexture)
		{
			GetTextureForMaterial(TexUse::Emissive, material, mat->pbr.emission_color.texture->file_index, state);
		}

		const ufbx_texture* aoTexture{ GetFileTexture(mat->pbr.ambient_occlusion.texture) };
		if (aoTexture)
		{
			GetTextureForMaterial(TexUse::AmbientOcclusion, material, mat->pbr.ambient_occlusion.texture->file_index, state);
		}

		state->Materials.emplace_back(std::move(material));
	}
}

void
ImportUfbxLights(const ufbx_scene* const scene, FBXImportState* const state)
{
	const u32 lightCount{ (u32)scene->lights.count };
	if (!lightCount) return;

	assert(lightCount == state->FBXLightNodes.size());
	state->Lights.resize(lightCount);
	log::Info("Importing %u lights", lightCount);
	for(u32 i{0}; i < lightCount; ++i)
	{
		const FBXLightNode& lightNode{ state->FBXLightNodes[i] };
		ufbx_light* fbxLight{ lightNode.Light };
		ufbx_node* node{ scene->nodes.data[lightNode.NodeIndex] };
		LightInitInfo& lightInfo{ state->Lights[i] };
		lightInfo.Color = { (f32)fbxLight->color.x, (f32)fbxLight->color.y, (f32)fbxLight->color.z };
		lightInfo.Intensity = (f32)fbxLight->intensity;
		lightInfo.Type =
			fbxLight->type == UFBX_LIGHT_POINT ? graphics::light::LightType::Point :
			(fbxLight->type == UFBX_LIGHT_SPOT ? graphics::light::LightType::Spot : graphics::light::LightType::Directional);
		assert(lightInfo.Type < graphics::light::LightType::Count);
		lightInfo.Range = 1.f;
		lightInfo.InnerConeAngle = (f32)fbxLight->inner_angle;
		lightInfo.OuterConeAngle = (f32)fbxLight->outer_angle;

		lightInfo.Transform.Position = { (f32)node->local_transform.translation.x, (f32)node->local_transform.translation.y,
			(f32)node->local_transform.translation.z };
		lightInfo.Transform.Rotation = { (f32)node->local_transform.rotation.x, (f32)node->local_transform.rotation.y,
			(f32)node->local_transform.rotation.z, (f32)node->local_transform.rotation.w };
		lightInfo.Transform.Scale = { (f32)node->local_transform.scale.x, (f32)node->local_transform.scale.y,
			(f32)node->local_transform.scale.z };

		lightInfo.Name = fbxLight->name.data;
		// should also process cast_shadows
	}
}

void
ImportUfbxMesh(ufbx_node* node, LodGroup& lodGroup, FBXImportState* const state)
{
	// node - LodGroup

	ufbx_mesh* m{ node->mesh };
	ufbx_matrix toWorld{node->geometry_to_world};
	ufbx_matrix normalMat{ ufbx_matrix_for_normals(&toWorld) };
	const char* name{ node->name.data };

	log::Info("Importing UFBX mesh: %s", name);

	Vec<u32> triangleIndices(m->max_face_triangles * 3);

	// Iterate over each face using the specific material
	for (const ufbx_mesh_part& part : m->material_parts)
	{
		if (part.num_triangles == 0 || part.num_faces == 0)
		{
			log::Warn("Mesh part contains no triangles");
			continue;
		}
		// Mesh
		Mesh mesh{};
		mesh.Name = std::string(name) + "_part_" + std::to_string(part.index);
		mesh.LodThreshold = 0.f;

		Vec<Vertex>& vertices{ mesh.Vertices };
		Vec<v3>& vertexPositions{ mesh.Positions };
		Vec<u32> partIndices{};

		for (u32 faceIdx : part.face_indices)
		{
			ufbx_face face{ m->faces[faceIdx] };

			// Triangulate the face
			u32 triangleCount{ ufbx_triangulate_face(triangleIndices.data(), triangleIndices.size(), m, face) };
			const u32 vertexCount{ triangleCount * 3 };

			// Iterate over each triangle corner contiguously
			for (u32 i{ 0 }; i < vertexCount; ++i)
			{
				u32 index{ triangleIndices[i] };

				Vertex v{};
				ufbx_vec3 pos{ ufbx_transform_position(&toWorld, m->vertex_position[index]) };
				//ufbx_vec3 normal{ m->vertex_normal[index] };
				ufbx_vec3 normal{ ufbx_vec3_normalize(ufbx_transform_direction(&normalMat, m->vertex_normal[index])) };
				ufbx_vec2 uv{ m->vertex_uv[index] };
				v.Position = { (f32)pos.x, (f32)pos.y, (f32)pos.z };
				v.Normal = { (f32)normal.x, (f32)normal.y, (f32)normal.z };
				v.UV = { (f32)uv.x, 1.f - (f32)uv.y };
				
				vertices.emplace_back(v);
				vertexPositions.emplace_back(v.Position);
			}
		}

		if(state->ImportSettings.ColliderFromGeometry) 
			state->JoltMeshShapes.emplace_back(physics::CreateJoltMeshFromVertices(vertexPositions, state->ImportSettings));

		if (m->vertex_tangent.exists && !state->ImportSettings.CalculateTangents)
		{

			if (m->vertex_bitangent.exists) 
			{

			}
		}

		if (m->vertex_color.exists)
		{
			//Vec<v3>& colors{ mesh.Colors };

			//for (u32 i{ 0 }; i < triangleCount * 3; ++i)
			//{
			//	ufbx_vec4 color{ m->vertex_color[index] };
			//	v3 col{ (f32)color.x, (f32)color.y, (f32)color.z };
			//	colors.emplace_back(col);
			//	v.Red = static_cast<u8>(math::Clamp(col.x, 0.0f, 1.0f) * 255.0f);
			//	v.Green = static_cast<u8>(math::Clamp(col.y, 0.0f, 1.0f) * 255.0f);
			//	v.Blue = static_cast<u8>(math::Clamp(col.z, 0.0f, 1.0f) * 255.0f);
			//}
		}

		assert(vertices.size() == part.num_triangles * 3);

		// Generate the index buffer
		ufbx_vertex_stream streams[1] {
			{ vertices.data(), vertices.size(), sizeof(Vertex) },
		};

		Vec<u32> indices(part.num_triangles * 3);

		u32 uniqueVertexCount{ (u32)ufbx_generate_indices(streams, 1, indices.data(), indices.size(), nullptr, nullptr) };

		log::Info("Mesh '%s' has %u unique vertices", name, uniqueVertexCount);

		// Create vertex and index buffers
		mesh.RawIndices = std::move(indices);


		// materials
		ufbx_material* fbxMat{ part.index < m->materials.count ? m->materials[part.index] : nullptr };
		if (fbxMat)
		{
			const u32 materialIdx = (u32)fbxMat->typed_id;
			assert(materialIdx < state->Materials.size());
			mesh.MaterialIndices.emplace_back(materialIdx);
		}
		else
		{
			// assign invalid id which indicates the mesh should use the default material
			mesh.MaterialIndices.emplace_back(id::INVALID_ID);
		}

		mesh.MaterialUsed.emplace_back(1);

		for (const Vertex& v : mesh.Vertices)
		{
			mesh.Positions.emplace_back(v.Position);
			mesh.Normals.emplace_back(v.Normal);
			mesh.UvSets.resize(1);
			mesh.UvSets[0].emplace_back(v.UV);
		}

		lodGroup.Meshes.emplace_back(mesh);
	}
	state->MeshNames.emplace_back(m->name.data, m->name.length);
}

//	data layout: 
//	... todo
//	All Texture Handles (till the end of the file or assume count * TextureUsage::Count?)
void
CreateGeometryMetadata(AssetPtr asset, const FBXImportState* const state, const std::filesystem::path& metadataPath)
{
	assert(metadataPath.extension().string() == ASSET_METADATA_EXTENSION);

	const u32 textureCount{ (u32)state->AllTextureHandles.size() };
	if (textureCount == 0)
	{
		log::Warn("No textures found, couldn't generate metadata");
		return;
	}

	const u32 metadataSize{ textureCount * sizeof(u64) };
	u8* buffer{ new u8[metadataSize] };
	util::BlobStreamWriter blob{ buffer, metadataSize };

	for (const AssetHandle texHandle : state->AllTextureHandles)
	{
		blob.Write<u64>(texHandle.id);
	}

	assert(blob.Offset() == metadataSize);
	std::ofstream file{ metadataPath, std::ios::out | std::ios::binary };
	assert(file);
	if (!file) return;
	file.write(reinterpret_cast<const char*>(buffer), metadataSize);

	asset->RelatedCount++;
}

void
ImportMeshes(ufbx_scene* scene, FBXImportState* const state, const std::filesystem::path& outPath)
{
	MeshGroup meshGroup{};
	meshGroup.Name = scene->metadata.filename.data;
	meshGroup.Name = "MeshGroup";

	LodGroup lodGroup{};
	//lodGroup.Name = node->name.data;
	lodGroup.Name = "testing lod group";

	for (ufbx_node* node : scene->nodes)
	{
		if (node->is_root) continue;

		if (node->element.type == UFBX_ELEMENT_LOD_GROUP)
		{
			// each child is an LOD level
			for (u32 i = 0; i < node->children.count; ++i)
			{
				ufbx_node* child = node->children.data[i];
				if (child->mesh)
				{
					ImportUfbxMesh(child, lodGroup, state);
				}
			}
		}
		else if (node->mesh)
		{
			ImportUfbxMesh(node, lodGroup, state);
		}
	}
	meshGroup.LodGroups.emplace_back(lodGroup);
	state->LodGroups.emplace_back(lodGroup);



	ProcessMeshGroupData(meshGroup, state->ImportSettings);
	//PackGeometryData(meshGroup, outData);
	//PackGeometryDataForEditor(meshGroup, data, outPath);
	//SaveGeometry(data, path.replace_extension(".geom"));
	PackGeometryForEngine(meshGroup, outPath);
}

const std::filesystem::path
ImportFBX(std::filesystem::path path, AssetPtr asset, const std::filesystem::path& importedPath)
{
	log::Info("Importing mesh: %s", path.string().c_str());

	const std::string originalFilename{ path.stem().string() };

	std::unique_ptr<FBXImportState> state{ std::make_unique<FBXImportState>() };
	state->Name = originalFilename;
	state->ModelResourcePath = importedPath.empty() ? editor::project::GetResourceDirectory() / "ImportedScenes" / state->Name : importedPath.parent_path();
	state->ModelSourcePath = path.parent_path();
	std::filesystem::create_directory(state->ModelResourcePath);

	std::filesystem::path importedAssetPath{ importedPath.empty() ? state->ModelResourcePath / state->Name : importedPath };
	importedAssetPath.replace_extension(".mesh");

	state->OutModelFile = importedAssetPath;
	state->ImportSettings = editor::assets::GetGeometryImportSettings();

	ufbx_load_opts opts{};
	opts.target_axes = ufbx_axes_right_handed_y_up;
	opts.target_unit_meters = 1.0f;
	ufbx_error error;
	ufbx_scene* scene = ufbx_load_file(path.string().c_str(), &opts, &error);
	if (!scene) 
	{
		log::Error("Failed to load: %s\n", error.description.data);
		return importedAssetPath;
	}

	//state->Transforms = Array<ecs::component::LocalTransform>{ scene->nodes.count };
	state->FBXLightNodes = Array<FBXLightNode>{ scene->lights.count };
	u32 lightIdx{ 0 };
	//state->Transforms.resize(scene->nodes.count);

	// list all nodes in the scene
	for (u32 i{ 0 }; i < scene->nodes.count; i++) 
	{
		ufbx_node* node = scene->nodes.data[i];
		if (node->is_root) continue;
		/*state->Transforms[i].Position = { (f32)node->local_transform.translation.x, (f32)node->local_transform.translation.y, 
			(f32)node->local_transform.translation.z };
		state->Transforms[i].Rotation = { (f32)node->local_transform.rotation.x, (f32)node->local_transform.rotation.y, 
			(f32)node->local_transform.rotation.z, (f32)node->local_transform.rotation.w };
		state->Transforms[i].Scale = { (f32)node->local_transform.scale.x, (f32)node->local_transform.scale.y, 
			(f32)node->local_transform.scale.z };*/

		if (node->light) state->FBXLightNodes[lightIdx++] = { node->light, i };
	}

	FBXImportState* const statePtr{ state.get() };
	ImportImages(scene, "", statePtr);
	ImportFBXMaterials(scene, statePtr);
	ImportMeshes(scene, statePtr, importedAssetPath);
	ImportUfbxLights(scene, statePtr);

	ufbx_free_scene(scene);

	std::filesystem::path mtPath{ importedAssetPath };
	mtPath.replace_extension(ASSET_METADATA_EXTENSION);
	CreateGeometryMetadata(asset, statePtr, mtPath);
	
	editor::assets::ViewFBXImportSummary(state);

	return importedAssetPath;
}

const std::filesystem::path
ImportPhysicsShape([[maybe_unused]] std::filesystem::path path, [[maybe_unused]] AssetPtr asset, [[maybe_unused]] const std::filesystem::path& importedPath)
{
	assert(false);
	return {};
}

const std::filesystem::path
ImportShader([[maybe_unused]] std::filesystem::path path, [[maybe_unused]] AssetPtr asset, [[maybe_unused]] const std::filesystem::path& importedPath)
{
	assert(std::filesystem::exists(path));
	shaders::ShaderFileInfo info{};
	const std::string basePathStr{ path.parent_path().append("").string()};
	const std::string filename{ path.filename().string() };
	info.File = filename.c_str();
	info.EntryPoint = "VS";
	info.Type = shaders::ShaderType::Vertex;
	const char* shaderBasePath{ basePathStr.c_str() };

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
		vertexShaders.emplace_back(std::move(shaders::CompileShader(info, shaderBasePath, extraArgs)));
		assert(vertexShaders.back().get());
		vertexShaderPtrs.emplace_back(vertexShaders.back().get());
	}

	info.EntryPoint = "PS";
	info.Type = shaders::ShaderType::Pixel;
	Vec<std::unique_ptr<u8[]>> pixelShaders{};
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

		pixelShaders.emplace_back(std::move(shaders::CompileShader(info, shaderBasePath, extraArgs)));
		assert(pixelShaders.back().get());
		psShaderPtrs.emplace_back(pixelShaders.back().get());
	}
	assert(pixelShaders.back().get());

	asset->AdditionalData = content::AddShaderGroup(vertexShaderPtrs.data(), (u32)vertexShaderPtrs.size(), keys.data());
	asset->AdditionalData2 = content::AddShaderGroup(psShaderPtrs.data(), (u32)psShaderPtrs.size(), psKeys.data());
	asset->OriginalFilePath = path;
	std::filesystem::path compiledPath{ editor::project::GetResourceDirectory() / "Shaders" / filename };
	compiledPath.replace_extension(".sd");
	//TODO: shaders::SaveCompiledShader();
	asset->ImportedFilePath = compiledPath;
	return compiledPath;
}

using AssetImporter = const std::filesystem::path(*)(std::filesystem::path path, AssetPtr asset, const std::filesystem::path& importedPath);
constexpr std::array<AssetImporter, AssetType::Count> assetImporters {
	ImportUnknown,
	ImportFBX,
	ImportTexture,
	ImportPhysicsShape,
	ImportUnknown,
	ImportUnknown,
	ImportUnknown,
	ImportShader,
	ImportUnknown
};

} // anonymous namespace

// Import an asset that doesn't have an entry in the asset registry yet (called when a new file gets added to the project)
// importedPath can be empty if the resource should be placed in a generic path for its asset type
AssetHandle 
ImportAsset(std::filesystem::path path, const std::filesystem::path& importedPath)
{
	assert(std::filesystem::exists(path));
	assert(path.has_extension());

	std::string extension = path.extension().string();
	auto it = assetTypeFromExtension.find(extension);
	if (it != assetTypeFromExtension.end())
	{
		AssetType::type assetType{ it->second };
		Asset* asset = new Asset{ assetType, path, {} };
		const std::filesystem::path assetPath{ assetImporters[assetType](path, asset, importedPath) };
		asset->ImportedFilePath = assetPath;
		if (!std::filesystem::exists(assetPath))
		{
			std::string filePath{ path.string() };
			log::Error("Error importing asset [%s]", filePath.data());
		}

		const bool isReimporting{ !importedPath.empty() };
		if (isReimporting)
		{
			AssetHandle handle{ assets::GetHandleFromImportedPath(importedPath) };
			assets::DeregisterAsset(handle);
		}

		return assets::RegisterAsset(asset);
	}

	log::Error("Unknown asset type for file: %s", path.string().c_str());
	return content::INVALID_HANDLE;
}

// if the asset has ImportedFilePath, reimporting occurs
void
ImportAsset(AssetHandle handle)
{
	AssetPtr asset{ assets::GetAsset(handle) };
	assert(asset);
	asset->ImportedFilePath = assetImporters[asset->Type](asset->OriginalFilePath, asset, asset->ImportedFilePath);

	if (!std::filesystem::exists(asset->ImportedFilePath))
	{
		std::string filePath{ asset->OriginalFilePath.string() };
		log::Error("Error importing asset %u [%s]", handle.id, filePath.data());
	}
}

}