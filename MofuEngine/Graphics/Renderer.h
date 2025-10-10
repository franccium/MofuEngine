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
	u32 LightSetIdx{ 0 };
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
	enum Flags : u32
	{
		None = 0u,
		Vertex = (1u << 0),
		Pixel = (1u << 1),
		Domain = (1u << 2),
		Hull = (1u << 3),
		Geometry = (1u << 4),
		Compute = (1u << 5),
		Amplification = (1u << 6),
		Mesh = (1u << 7)
	};
};

struct MaterialFlags
{
	enum Flags : u32
	{
		None = 0u,
		TextureRepeat = (1u << 0),
		NoFaceCulling = (1u << 1),
		BlendAlpha = (1u << 2),
		BlendAdditive = (1u << 3),
		PremultipliedAlpha = (1u << 4),
		DepthBufferDisabled = (1u << 5),
	};
};

struct ShaderType
{
	enum Type : u32
	{
		Vertex = 0,
		Pixel,
		Domain,
		Hull,
		Geometry,
		Compute,
		Amplification,
		Mesh,
		Library,

		Count = Library,
		CountWithLibrary = Count + 1
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
	id_t AddedMaterialID;
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
	id_t* TextureIDs{ nullptr };
	MaterialSurface Surface{}; // TODO: wastes a lot of bytes, even though most models would use textures anyways
	MaterialType::type Type{ MaterialType::Opaque };
	u32 TextureCount{ 0 }; // NOTE: textures are optional, texture_count may be 0, and texture_ids null
	id_t ShaderIDs[ShaderType::Count]{ id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID, id::INVALID_ID };
	MaterialFlags::Flags MaterialFlags{ MaterialFlags::None };
};

struct AmbientLightInitInfo
{
	f32 Intensity{ 1.f };
	id_t DiffuseTextureID{ id::INVALID_ID };
	id_t SpecularTextureID{ id::INVALID_ID };
	id_t BRDFLutTextureID{ id::INVALID_ID };
	u32 LightSetIdx{ id::INVALID_ID };
};

void SetCurrentFrameInfo(FrameInfo info);
const FrameInfo& GetCurrentFrameInfo();

bool Initialize(GraphicsPlatform platform);
void Shutdown();

Surface CreateSurface(platform::Window window);
void RemoveSurface(surface_id id);

Camera CreateCamera(CameraInitInfo info);
void RemoveCamera(camera_id id);
const Camera& GetMainCamera();

const char* GetEngineShadersPath();
const char* GetEngineShadersPath(GraphicsPlatform platform);
const char* GetDebugEngineShadersPath();
const char* GetDebugEngineShadersPath(GraphicsPlatform platform);

id_t AddSubmesh(const u8*& data);
void RemoveSubmesh(id_t id);

id_t AddTexture(const u8* const data);
void RemoveTexture(id_t id);
void GetDescriptorIndices(const id_t* const textureIDs, u32 count, u32* const outIndices); //TODO: something more elegant

Vec<ecs::Entity>& GetVisibleEntities();

id_t AddMaterial(MaterialInitInfo info);
void RemoveMaterial(id_t id);
MaterialInitInfo GetMaterialReflection(id_t id);

id_t AddRenderItem(ecs::Entity entityID, id_t geometryContentID, u32 materialCount, const id_t materialID);
RenderItemInfo AddRenderItemRecoverInfo(ecs::Entity entityID, id_t geometryContentID, u32 materialCount, const id_t materialID);
void RemoveRenderItem(id_t id);
void UpdateRenderItemData(id_t oldRenderItemID, id_t newRenderItemID);
}