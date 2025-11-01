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
ImportUnknown([[maybe_unused]] std::filesystem::path path, [[maybe_unused]] AssetPtr asset)
{
	assert(false);
	return {};
}

void
CreateBRDFLut(const texture::TextureData& data, std::filesystem::path targetPath)
{
	texture::TextureData lutData{};
	lutData.ImportSettings = data.ImportSettings;
	lutData.ImportSettings.Compress = false;
	lutData.ImportSettings.MipLevels = 1;

	texture::ComputeBRDFIntegrationLUT(&lutData);

	if (lutData.Info.ImportError != texture::ImportError::Succeeded)
	{
		log::Error("BRDF LUT creation error: %s", texture::TEXTURE_IMPORT_ERROR_STRING[data.Info.ImportError]);
		return;
	}

	PackTextureForEngine(lutData, targetPath);
	Asset* asset = new Asset{ content::AssetType::Texture, targetPath, targetPath };
	asset->AdditionalData2 = AssetFlag::Flag::IsBRDFLut;
	std::filesystem::path metadataPath{ targetPath.replace_extension(".mt") };
	PackTextureForEditor(lutData, metadataPath);

	assets::RegisterAsset(asset);

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
}

void
CreateTextureAssetIcon(AssetPtr asset)
{
	//TODO: a generic metadata file for this stuff?
	std::filesystem::path iconPath{ asset->ImportedFilePath };
	iconPath.replace_extension(".mt");
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
ImportTexture(std::filesystem::path path, AssetPtr asset)
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

	//Texture diffuseIBLCubemap = null;
	if (data.ImportSettings.PrefilterCubemap && data.Info.Flags & TextureFlags::IsCubeMap)
	{
		texture::TextureData diffuseData{};
		diffuseData.SubresourceData = new u8[data.SubresourceSize];
		memcpy(diffuseData.SubresourceData, data.SubresourceData, data.SubresourceSize);
		diffuseData.SubresourceSize = data.SubresourceSize;
		diffuseData.Icon = new u8[data.IconSize];
		memcpy(diffuseData.Icon, data.Icon, data.IconSize);
		diffuseData.IconSize = data.IconSize;
		diffuseData.Info = data.Info;
		diffuseData.ImportSettings = data.ImportSettings;
			
		texture::PrefilterDiffuseIBL(&diffuseData);
		texture::PrefilterSpecularIBL(&data);

		
		std::filesystem::path texturePath{ editor::project::GetResourceDirectory() / "Cubemaps" };
		std::filesystem::path diffPath{ texturePath };
		diffPath.append(path.stem().string() + ".tex");
		asset->ImportedFilePath = diffPath;
		asset->RelatedCount = diffuseData.ImportSettings.FileCount;

		std::filesystem::path specPath{ texturePath };
		specPath.append(path.stem().string() + "spec.tex");
		Asset* specularAsset = new Asset{ content::AssetType::Texture, path, {} };
		specularAsset->ImportedFilePath = specPath;
		specularAsset->RelatedCount = diffuseData.ImportSettings.FileCount;
		content::AssetHandle specularHandle{ assets::RegisterAsset(specularAsset) };
		diffuseData.Info.IBLPair = specularHandle;

		//TODO: somehow get the iblpair saved for the specular metadata
		PackTextureForEngine(diffuseData, diffPath);
		std::filesystem::path metadataPath{ diffPath.replace_extension(".mt") };
		PackTextureForEditor(diffuseData, metadataPath);

		PackTextureForEngine(data, specPath);
		std::filesystem::path specMetadataPath{ specPath.replace_extension(".mt") };
		PackTextureForEditor(data, specMetadataPath);

		std::filesystem::path brdfLutPath{ diffPath.replace_filename(diffPath.stem().string() + "brdf_lut.tex") };
		CreateBRDFLut(diffuseData, brdfLutPath);

		std::unique_ptr<u8[]> iconBuffer{};
		u64 iconSize{};
		content::assets::GetTextureIconData(metadataPath, iconSize, iconBuffer);
		if (iconSize != 0)
		{
			id_t iconId{ graphics::ui::AddIcon(iconBuffer.get()) };
			asset->AdditionalData = iconId;
		}

		delete[] diffuseData.SubresourceData;
		delete[] data.SubresourceData;
		delete[] diffuseData.Icon;
		delete[] data.Icon;

		return asset->ImportedFilePath;
	}

	std::filesystem::path texturePath{ editor::project::GetResourceDirectory() / "Textures" };
	texturePath.append(path.stem().string() + ".tex");
	asset->ImportedFilePath = texturePath;
	asset->RelatedCount = data.ImportSettings.FileCount;
	PackTextureForEngine(data, texturePath);
	std::filesystem::path metadataPath{ texturePath.replace_extension(".mt") };
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
ImportImageFromBytes(const u8* const bytes, u64 size, const char* fileExtension, std::filesystem::path targetPath)
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
		log::Error("Texture import error: ", data.Info.ImportError);
	}

	PackTextureForEngine(data, targetPath);
	PackTextureForEditor(data, targetPath);
}

void
ImportImages(const ufbx_scene* fbxScene, const std::string_view basePath, FBXImportState& state)
{
	state.Textures.resize(fbxScene->texture_files.count);
	state.SourceImages.resize(fbxScene->texture_files.count);
	state.ImageFiles.resize(fbxScene->texture_files.count);
	for (u32 textureIdx{ 0 }; textureIdx < fbxScene->texture_files.count; ++textureIdx)
	{
		const ufbx_texture_file& fbxTexFile{ fbxScene->texture_files[textureIdx] };

		std::filesystem::path texturePath{ fbxTexFile.filename.data };
		if (texturePath.is_absolute()) 
		{
			texturePath = texturePath.filename();
			texturePath = editor::project::GetAssetDirectory() / std::filesystem::path{ "ab" } / texturePath;
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
				state.Errors |= FBXImportState::ErrorFlags::InvalidTexturePath;
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
				state.Textures[textureIdx] = {};
				state.SourceImages[textureIdx] = {};
				state.ImageFiles[textureIdx] = {};
			}
		}

		if (contentSize > 0)
		{
			std::filesystem::path targetPath{ editor::project::GetAssetDirectory() / "ImportedTextures" };
			std::filesystem::path assetPath{ editor::project::GetResourceDirectory() / "ImportedTextures" };
			if (!std::filesystem::exists(targetPath)) 
			{
				std::filesystem::create_directory(targetPath);
			}
			//char name[16];
			//sprintf_s(name, "%u%s\0", textureIdx, texturePath.extension().string().data());
			targetPath /= texturePath.filename();
			assetPath /= texturePath.filename();
			assetPath.replace_extension(".tex");
			ImportImageFromBytes(data.get(), contentSize, texturePath.extension().string().data(), targetPath);
			state.Textures[textureIdx] = {};
			state.SourceImages[textureIdx] = {};
			state.ImageFiles[textureIdx] = assetPath.string();

			Asset* asset = new Asset{ AssetType::Texture, targetPath, assetPath };
			assets::RegisterAsset(asset);
		}
	}
}

void
GetTextureForMaterial(editor::material::TextureUsage::Usage textureUsage, editor::material::EditorMaterial& material, u32 fileIndex, FBXImportState& state)
{
	//TODO: reuse already loaded

	if (!state.ImportSettings.ImportEmbeddedTextures) return;
	u32 sourceImageIndex{ state.SourceImages[fileIndex] };
	if (!state.ImageFiles[sourceImageIndex].empty())
	{
		material.TextureIDs[textureUsage] = content::CreateResourceFromAsset(state.ImageFiles[sourceImageIndex], content::AssetType::Texture);
	}
	else
	{
		log::Warn("Couldn't find texture for %u, idx: %u %u", textureUsage, fileIndex, sourceImageIndex);
		state.Errors |= FBXImportState::ErrorFlags::MaterialTextureNotFound;
		material.TextureIDs[textureUsage] = id::INVALID_ID;
	}
}

void
ImportFBXMaterials(ufbx_scene* scene, FBXImportState& state)
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

		state.Materials.emplace_back(std::move(material));
	}
}

void
ImportUfbxMesh(ufbx_node* node, LodGroup& lodGroup, FBXImportState& state)
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

		state.JoltMeshShapes.emplace_back(physics::CreateJoltMeshFromVertices(vertexPositions, state.ImportSettings));

		if (m->vertex_tangent.exists && !state.ImportSettings.CalculateTangents)
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
			assert(materialIdx < state.Materials.size());
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
	state.MeshNames.emplace_back(m->name.data, m->name.length);
}

void
ImportMeshes(ufbx_scene* scene, FBXImportState& state)
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
				if (!child->mesh) continue;

				ImportUfbxMesh(child, lodGroup, state);
			}
		}
		else if (node->mesh)
		{
			ImportUfbxMesh(node, lodGroup, state);
		}
	}
	meshGroup.LodGroups.emplace_back(lodGroup);
	state.LodGroups.emplace_back(lodGroup);

	ufbx_free_scene(scene);


	ProcessMeshGroupData(meshGroup, state.ImportSettings);
	//PackGeometryData(meshGroup, outData);
	//PackGeometryDataForEditor(meshGroup, data);
	//SaveGeometry(data, path.replace_extension(".geom"));
	PackGeometryForEngine(meshGroup, state.OutModelFile);
}

const std::filesystem::path
ImportFBX(std::filesystem::path path, AssetPtr asset)
{
	log::Info("Importing mesh: %s", path.string().c_str());

	FBXImportState state{};
	state.FbxFile = path.filename().string();

	std::filesystem::path importedAssetPath{ editor::project::GetResourceDirectory() / "Meshes" };
	importedAssetPath.append(state.FbxFile);
	importedAssetPath.replace_extension(".mesh");

	state.OutModelFile = importedAssetPath;
	state.ImportSettings = editor::assets::GetGeometryImportSettings();

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
	// list all nodes in the scene
	for (u32 i{ 0 }; i < scene->nodes.count; i++) 
	{
		ufbx_node* node = scene->nodes.data[i];
		if (node->is_root) continue;

		log::Info("Object: %s\n", node->name.data);
		if (node->mesh) {
			log::Info("-> mesh with %zu faces\n", node->mesh->faces.count);
		}
	}

	if (state.ImportSettings.ImportEmbeddedTextures)
	{
		ImportImages(scene, "", state);
	}
	ImportFBXMaterials(scene, state);
	ImportMeshes(scene, state);
	
	editor::assets::ViewFBXImportSummary(state);

	return importedAssetPath;
}

const std::filesystem::path
ImportPhysicsShape(std::filesystem::path path, AssetPtr asset)
{
	assert(false);
	return {};
}

using AssetImporter = const std::filesystem::path(*)(std::filesystem::path path, AssetPtr asset);
constexpr std::array<AssetImporter, AssetType::Count> assetImporters {
	ImportUnknown,
	ImportFBX,
	ImportTexture,
	ImportPhysicsShape,
	ImportUnknown,
	ImportUnknown,
	ImportUnknown,
	ImportUnknown
};

} // anonymous namespace

// Import an asset that doesn't have an entry in the asset registry yet (called when a new file gets added to the project)
void 
ImportAsset(std::filesystem::path path)
{
	assert(std::filesystem::exists(path));
	assert(path.has_extension());

	std::string extension = path.extension().string();
	auto it = assetTypeFromExtension.find(extension);
	if (it != assetTypeFromExtension.end())
	{
		AssetType::type assetType{ it->second };
		Asset* asset = new Asset{ assetType, path, {} };
		const std::filesystem::path importedPath{ assetImporters[assetType](path, asset) };
		asset->ImportedFilePath = importedPath;
		if (!std::filesystem::exists(asset->ImportedFilePath))
		{
			std::string filePath{ path.string() };
			log::Error("Error importing asset [%s]", filePath.data());
		}
		assets::RegisterAsset(asset);
		return;
	}

	log::Error("Unknown asset type for file: %s", path.string().c_str());
}

void
ImportAsset(AssetHandle handle)
{
	AssetPtr asset{ assets::GetAsset(handle) };
	assert(asset);
	asset->ImportedFilePath = assetImporters[asset->Type](asset->OriginalFilePath, asset);

	if (!std::filesystem::exists(asset->ImportedFilePath))
	{
		std::string filePath{ asset->OriginalFilePath.string() };
		log::Error("Error importing asset %u [%s]", handle.id, filePath.data());
	}
}

}