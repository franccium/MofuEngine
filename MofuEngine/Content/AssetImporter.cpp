#include "AssetImporter.h"
#include <functional>
#include <array>
#include <unordered_map>
#include "Utilities/Logger.h"
#include "External/ufbx/ufbx.h"

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
ImportMesh(std::filesystem::path path)
{
	log::Info("Importing mesh: %s", path.string().c_str());

	ufbx_load_opts opts{};
	opts.target_axes = ufbx_axes_right_handed_y_up;
	opts.target_unit_meters = 1.0f;
	ufbx_error error;
	ufbx_scene* scene = ufbx_load_file(path.string().c_str(), &opts, &error);
	if (!scene) {
		log::Error("Failed to load: %s\n", error.description.data);
		return;
	}

	// Use and inspect `scene`, it's just plain data!



	// Let's just list all objects within the scene for example:
	for (size_t i = 0; i < scene->nodes.count; i++) {
		ufbx_node* node = scene->nodes.data[i];
		if (node->is_root) continue;

		log::Info("Object: %s\n", node->name.data);
		if (node->mesh) {
			log::Info("-> mesh with %zu faces\n", node->mesh->faces.count);
		}
	}

	ufbx_free_scene(scene);
}

using AssetImporter = void(*)(std::filesystem::path path);
constexpr std::array<AssetImporter, AssetType::Count> assetImporters {
	ImportUnknown,
	ImportMesh,
};

const std::unordered_map<std::string_view, AssetType::type> assetTypeFromExtension {
	{ ".fbx", AssetType::Mesh },
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