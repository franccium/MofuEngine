#include "AssetImporter.h"
#include <functional>
#include <array>
#include <unordered_map>
#include "Utilities/Logger.h"
#include "External/ufbx/ufbx.h"
#include "Content/TextureImport.h"
#include "Editor/AssetPacking.h"

namespace mofu::content {
namespace {

void
ImportUnknown([[maybe_unused]] std::filesystem::path path)
{
	assert(false);
}

//TODO: import fbx
//// GPU vertex representation.
//// In practice you would need to use more compact custom vector types as
//// by default `ufbx_real` is 64 bits wide.
//typedef struct Vertex {
//    ufbx_vec3 position;
//    ufbx_vec3 normal;
//    ufbx_vec2 uv;
//} Vertex;
//
//void convert_mesh_part(ufbx_mesh* mesh, ufbx_mesh_part* part)
//{
//    size_t num_triangles = part->num_triangles;
//    Vertex* vertices = calloc(num_triangles * 3, sizeof(Vertex));
//    size_t num_vertices = 0;
//
//    // Reserve space for the maximum triangle indices.
//    size_t num_tri_indices = mesh->max_face_triangles * 3;
//    uint32_t* tri_indices = calloc(num_tri_indices, sizeof(uint32_t));
//
//    // Iterate over each face using the specific material.
//    for (size_t face_ix = 0; face_ix < part->num_faces; face_ix++) {
//        ufbx_face face = mesh->faces.data[part->face_indices.data[face_ix]];
//
//        // Triangulate the face into `tri_indices[]`.
//        uint32_t num_tris = ufbx_triangulate_face(tri_indices, num_tri_indices, mesh, face);
//
//        // Iterate over each triangle corner contiguously.
//        for (size_t i = 0; i < num_tris * 3; i++) {
//            uint32_t index = tri_indices[i];
//
//            Vertex* v = &vertices[num_vertices++];
//            v->position = ufbx_get_vertex_vec3(&mesh->vertex_position, index);
//            v->normal = ufbx_get_vertex_vec3(&mesh->vertex_normal, index);
//            v->uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, index);
//        }
//    }
//
//    // Should have written all the vertices.
//    free(tri_indices);
//    assert(num_vertices == num_triangles * 3);
//
//    // Generate the index buffer.
//    ufbx_vertex_stream streams[1] = {
//        { vertices, num_vertices, sizeof(Vertex) },
//    };
//    size_t num_indices = num_triangles * 3;
//    uint32_t* indices = calloc(num_indices, sizeof(uint32_t));
//
//    // This call will deduplicate vertices, modifying the arrays passed in `streams[]`,
//    // indices are written in `indices[]` and the number of unique vertices is returned.
//    num_vertices = ufbx_generate_indices(streams, 1, indices, num_indices, NULL, NULL);
//
//    create_vertex_buffer(vertices, num_vertices);
//    create_index_buffer(indices, num_indices);
//
//    free(indices);
//    free(vertices);
//}

void
ImportUfbxMesh(ufbx_node* node, LodGroup& lodGroup)
{
	// node - LodGroup

	ufbx_mesh* m{ node->mesh };
	ufbx_matrix toWorld{node->geometry_to_world};
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
		mesh.ElementType = ElementType::StaticNormalTexture; // TODO: temporary
		
		Vec<Vertex>& vertices{ mesh.Vertices };
		Vec<u32> partIndices{};

		for (u32 faceIdx : part.face_indices)
		{
			ufbx_face face{ m->faces[faceIdx] };

			// Triangulate the face
			u32 triangleCount{ ufbx_triangulate_face(triangleIndices.data(), triangleIndices.size(), m, face) };

			// Iterate over each triangle corner contiguously
			for (u32 i{ 0 }; i < triangleCount * 3; ++i)
			{
				u32 index{ triangleIndices[i] };

				Vertex v{};
				ufbx_vec3 pos{ m->vertex_position[index] }; // ufbx_transform_position(&toWorld, pos);
				ufbx_vec3 normal{ m->vertex_normal[index] };
				ufbx_vec2 uv{ m->vertex_uv[index] };
				v.Position = { (f32)pos.x, (f32)pos.y, (f32)pos.z };
				v.Normal = { (f32)normal.x, (f32)normal.y, (f32)normal.z };
				v.UV = { (f32)uv.x, (f32)uv.y };
				vertices.emplace_back(v);
			}
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
		mesh.MaterialIndices.resize(mesh.Indices.size(), 0);
		mesh.MaterialUsed.emplace_back(0);

		for (const Vertex& v : mesh.Vertices)
		{
			mesh.Positions.emplace_back(v.Position);
			mesh.Normals.emplace_back(v.Normal);
			mesh.UvSets.resize(1);
			mesh.UvSets[0].emplace_back(v.UV);
		}

		lodGroup.Meshes.emplace_back(mesh);
	}
}

void
ImportMesh(std::filesystem::path path)
{
	log::Info("Importing mesh: %s", path.string().c_str());

	ufbx_load_opts opts{};
	opts.target_axes = ufbx_axes_right_handed_y_up;
	opts.target_unit_meters = 1.0f;
	ufbx_error error;
	ufbx_scene* scene = ufbx_load_file(path.string().c_str(), &opts, &error);
	if (!scene) 
	{
		log::Error("Failed to load: %s\n", error.description.data);
		return;
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

	MeshGroup meshGroup{};
	meshGroup.Name = scene->metadata.filename.data;
	meshGroup.Name = path.stem().string() + ".model";

	/*for (ufbx_node* node : scene->nodes) 
	{
		if (!node->mesh || node->is_root) continue;

		ImportUfbxMesh(node, meshGroup);
	}*/

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

				ImportUfbxMesh(child, lodGroup);
			}
		}
		else if (node->mesh)
		{
			ImportUfbxMesh(node, lodGroup);
		}
	}
	meshGroup.LodGroups.emplace_back(lodGroup);

	ufbx_free_scene(scene);

	MeshGroupData data{};
	data.ImportSettings.CalculateNormals = false;
	data.ImportSettings.CalculateTangents = true;
	data.ImportSettings.MergeMeshes = true;
	data.ImportSettings.ImportAnimations = false;
	data.ImportSettings.ImportEmbeddedTextures = false;
	data.ImportSettings.ReverseHandedness = false;
	data.ImportSettings.SmoothingAngle = 0.f;
	ProcessMeshGroupData(meshGroup, data.ImportSettings);
	//PackGeometryData(meshGroup, outData);
	//PackGeometryDataForEditor(meshGroup, data);
	//SaveGeometry(data, path.replace_extension(".geom"));
	PackGeometryForEngine(meshGroup);
}

//public bool SetData(SliceArray3D slices, Slice icon, Texture iblPair)
//{
//	if (slices != null && slices.Count != 0 && slices.First().Count != 0 && slices.First().First().Count != 0)
//	{
//		Slices = slices;
//	}
//	else
//	{
//		return false;
//	}
//
//	var firstMip = Slices[0][0][0];
//	if (!HasValidDimensions(firstMip.Width, firstMip.Height, ArraySize, IsVolumeMap, FullPath))
//	{
//		return false;
//	}
//
//	if (icon == null)
//	{
//		Debug.Assert(!ImportSettings.Compress);
//		icon = firstMip;
//	}
//
//	Icon = BitmapHelper.CreateThumbnail(BitmapHelper.ImageFromSlice(icon, Format, IsNormalMap), ContentInfo.IconWidth, ContentInfo.IconWidth);
//
//	if (iblPair != null)
//	{
//		IsPrefilteredIBL = true;
//		IBLPair = iblPair;
//	}
//
//	return true;
//}
//
//public void SetTextureInfoFromData(Texture texture)
//{
//	texture.Width = Info.Width;
//	texture.Height = Info.Height;
//	texture.ArraySize = Info.ArraySize;
//	texture.MipLevels = Info.MipLevels;
//	texture.Format = (DXGI_FORMAT)Info.Format;
//	texture.Flags = (TextureFlags)Info.Flags;
//}

void
ImportTexture(std::filesystem::path path)
{
	texture::TextureData data{};
	data.ImportSettings.Files = path.string(); //TODO: char* 
	data.ImportSettings.FileCount = 1;
	data.ImportSettings.Compress = true;
	texture::Import(&data);
	if (data.Info.ImportError != texture::ImportError::Succeeded)
	{
		log::Error("Texture import error: ", data.Info.ImportError);
	}

	PackTextureForEngine(data, path.filename().string());
	PackTextureForEditor(data, path.filename().string());
	//textureData.SetTextureInfoFromData(texture);
	//if (!texture.SetData(textureData.GetSlices(), textureData.GetIcon(), diffuseIBLCubemap)) throw new InvalidDataException();
}

using AssetImporter = void(*)(std::filesystem::path path);
constexpr std::array<AssetImporter, AssetType::Count> assetImporters {
	ImportUnknown,
	ImportMesh,
	ImportTexture,
};

const std::unordered_map<std::string_view, AssetType::type> assetTypeFromExtension {
	{ ".fbx", AssetType::Mesh },
	{ ".png", AssetType::Texture },
	{ ".tga", AssetType::Texture },
	{ ".tiff", AssetType::Texture },
	{ ".tif", AssetType::Texture },
	{ ".dds", AssetType::Texture },
	{ ".hdr", AssetType::Texture },
	{ ".jpg", AssetType::Texture },
	{ ".jpeg", AssetType::Texture },
	{ ".bmp", AssetType::Texture },
	{ ".wav", AssetType::Audio },
};

} // anonymous namespace

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
		assetImporters[assetType](path);
		return;
	}

	log::Error("Unknown asset type for file: %s", path.string().c_str());
}


}