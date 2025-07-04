#pragma once
#include "CommonHeaders.h"
#include "GraphicsPlatform.h"
#include "Platform/Window.h"
#include "EngineAPI/Camera.h"
#include "ECS/Entity.h"
#include "UIRenderer.h"

// A high level renderer with function pointers to the set platform's implementation
namespace mofu::graphics {
struct FrameInfo
{
    f32 LastFrameTime{ 16.7f };
    f32 AverageFrameTime{ 0.f };
	u32 RenderItemCount{ 0 };
	camera_id CameraID{ id::INVALID_ID };
	f32* Thresholds{ nullptr };
	id_t* RenderItemIDs{ nullptr };	
};

DEFINE_TYPED_ID(surface_id)
class Surface
{
public:
    constexpr explicit Surface(surface_id id) : _id{ id } {}
    constexpr Surface() = default;
    constexpr surface_id GetID() const { return _id; }
    constexpr bool IsValid() const { return id::IsValid(_id); }

    void Resize(u32 width, u32 height) const;
    u32 Width() const;
    u32 Height() const;
    void Render(FrameInfo info) const;

private:
    surface_id _id{ id::INVALID_ID };
};

struct RenderSurface
{
    platform::Window window{};
    Surface surface;
};

struct CameraProperty
{
	enum Property : u32
	{
		UpVector,
		FieldOfView,
		AspectRatio,
		ViewWidth,
		ViewHeight,
		NearZ,
		FarZ,
		View,
		Projection,
		InverseProjection,
		ViewProjection,
		InverseViewProjection,
		ProjectionType,
		EntityId,

		Count
	};
};

struct CameraInitInfo
{
	id_t EntityID{ id::INVALID_ID };
	Camera::Type Type;
	v3 Up;
	union
	{
		f32 FieldOfView;
		f32 ViewWidth;
	};
	union
	{
		f32 AspectRatio;
		f32 ViewHeight;
	};
	f32 NearZ;
	f32 FarZ;
};

struct PerspectiveCameraInitInfo : public CameraInitInfo
{
	explicit PerspectiveCameraInitInfo(id_t id)
	{
		assert(id::IsValid(id));
		EntityID = id;
		Type = Camera::Type::Perspective;
		Up = v3{ 0.f, 1.f, 0.f };	
		FieldOfView = 0.25f;	
		AspectRatio = 16.f / 9.f;
		NearZ = 0.1f;
		FarZ = 1000.f;
	}
};

struct OrthographicCameraInitInfo : public CameraInitInfo
{
	explicit OrthographicCameraInitInfo(id_t id)
	{
		assert(id::IsValid(id));
		EntityID = id;
		Type = Camera::Type::Orthographic;
		Up = v3{ 0.f, 1.f, 0.f };
		ViewWidth = 1920;
		ViewHeight = 1080;
		NearZ = 0.1f;
		FarZ = 100.f;
	}
};

struct ShaderFlags
{
	enum flags : u32
	{
		None = 0x00,
		Vertex = 0x01,
		Hull = 0x02,
		Domain = 0x04,
		Geometry = 0x08,
		Pixel = 0x10,
		Compute = 0x20,
		Amplification = 0x40,
		Mesh = 0x80
	};
};

struct ShaderType
{
	enum type : u32
	{
		Vertex = 0,
		Hull,
		Domain,
		Geometry,
		Pixel,
		Compute,
		Amplification,
		Mesh,

		Count
	};
};

struct PrimitiveTopology
{
	enum type : u32
	{
		PointList = 0,
		LineList,
		LineStrip,
		TriangleList,
		TriangleStrip,

		Count
	};
};

struct MaterialType
{
	enum type : u32
	{
		Opaque,

		Count
	};
};

struct RenderItemInfo
{
	u32 AddedSubmeshCount{ 0 };
	u32 AddedMaterialCount{ 0 };
	id_t* AddedSubmeshIDs;
	id_t* AddedMaterialIDs;
};

struct MaterialSurface
{
	v4 BaseColor{ v4one };
	v3 Emissive{ v3zero };
	f32 EmissiveIntensity{ 1.f };
	f32 AmbientOcclusion{ 1.f };
	f32 Metallic{ 0.f };
	f32 Roughness{ 1.f };
};

struct MaterialInitInfo
{
	id_t* TextureIDs;
	MaterialSurface Surface; // TODO: wastes a lot of bytes, even though most models would use textures anyways
	MaterialType::type Type;
	u32 TextureCount; // NOTE: textures are optional, texture_count may be 0, and texture_ids null
	id_t ShaderIDs[ShaderType::Count]{ id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID };
};

struct TextureHandle
{

};

void SetCurrentFrameInfo(FrameInfo info);
const FrameInfo& GetCurrentFrameInfo();

bool Initialize(GraphicsPlatform platform);
void Shutdown();

Surface CreateSurface(platform::Window window);
void RemoveSurface(surface_id id);

Camera CreateCamera(CameraInitInfo info);
void RemoveCamera(camera_id id);

const char* GetEngineShadersPath();
const char* GetEngineShadersPath(GraphicsPlatform platform);

id_t AddSubmesh(const u8*& data);
void RemoveSubmesh(id_t id);

id_t AddTexture(const u8* const data);
void RemoveTexture(id_t id);

id_t AddMaterial(MaterialInitInfo info);
void RemoveMaterial(id_t id);

id_t AddRenderItem(ecs::Entity entityID, id_t geometryContentID, u32 materialCount, const id_t* const materialIDs);
RenderItemInfo AddRenderItemRecoverInfo(ecs::Entity entityID, id_t geometryContentID, u32 materialCount, const id_t* const materialIDs);
void RemoveRenderItem(id_t id);
}