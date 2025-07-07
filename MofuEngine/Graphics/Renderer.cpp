#include "Renderer.h"
#include "GraphicsPlatformInterface.h"
#include "D3D12/D3D12Interface.h"
#include "EngineAPI/Camera.h"

namespace mofu::graphics {
namespace {
constexpr const char* ENGINE_SHADERS_PATHS[]{
    ".\\shaders\\d3d12\\shaders.bin"
};

PlatformInterface gfxInterface;
FrameInfo frameInfo{};

bool SetupPlatformInterface(GraphicsPlatform platform)
{
    switch (platform)
    {
    case mofu::graphics::GraphicsPlatform::Direct3D12:
        d3d12::SetupPlatformInterface(gfxInterface);
        break;
    default:
        return false;
    }

    assert(gfxInterface.platform == platform);
    return true;
}

} // anonymous namespace

void SetCurrentFrameInfo(FrameInfo info)
{
    frameInfo = info;
}

const FrameInfo& GetCurrentFrameInfo()
{
    return frameInfo;
}

bool
Initialize(GraphicsPlatform platform)
{
    return SetupPlatformInterface(platform) && gfxInterface.initialize() && ui::Initialize(&gfxInterface);
}

void 
Shutdown()
{
    if (gfxInterface.platform != (GraphicsPlatform)-1) gfxInterface.shutdown();
}

Surface
CreateSurface(platform::Window window)
{
    return gfxInterface.surface.create(window);
}

void
RemoveSurface(surface_id id)
{
    gfxInterface.surface.remove(id);
}

Camera 
CreateCamera(CameraInitInfo info)
{
	return gfxInterface.camera.create(info);
}

void 
RemoveCamera(camera_id id)
{
	gfxInterface.camera.remove(id);
}

const char* 
GetEngineShadersPath()
{
    return ENGINE_SHADERS_PATHS[(u32)gfxInterface.platform];
}

const char*
GetEngineShadersPath(GraphicsPlatform platform)
{
    return ENGINE_SHADERS_PATHS[(u32)platform];
}

void 
Surface::Resize(u32 width, u32 height) const
{
    assert(IsValid());
    gfxInterface.surface.resize(_id, width, height);
}

u32 
Surface::Width() const
{
    assert(IsValid());
    return gfxInterface.surface.width(_id);
}

u32 
Surface::Height() const
{
    assert(IsValid());
    return gfxInterface.surface.height(_id);
}

void 
Surface::Render(FrameInfo info) const
{
    assert(IsValid());
    gfxInterface.surface.render(_id, info);
}


void 
Camera::Up(v3 up) const
{
	gfxInterface.camera.setProperty(_id, CameraProperty::UpVector, &up, sizeof(up));
}

void 
Camera::FieldOfView(f32 fov) const
{
	gfxInterface.camera.setProperty(_id, CameraProperty::FieldOfView, &fov, sizeof(fov));
}

void 
Camera::AspectRatio(f32 aspectRatio) const
{
	gfxInterface.camera.setProperty(_id, CameraProperty::AspectRatio, &aspectRatio, sizeof(aspectRatio));
}

void 
Camera::ViewWidth(f32 width) const
{
	gfxInterface.camera.setProperty(_id, CameraProperty::ViewWidth, &width, sizeof(width));
}

void 
Camera::ViewHeight(f32 height) const
{
	gfxInterface.camera.setProperty(_id, CameraProperty::ViewHeight, &height, sizeof(height));
}

void 
Camera::Range(f32 nearZ, f32 farZ) const
{
	gfxInterface.camera.setProperty(_id, CameraProperty::NearZ, &nearZ, sizeof(nearZ));
	gfxInterface.camera.setProperty(_id, CameraProperty::FarZ, &farZ, sizeof(farZ));
}

m4x4 
Camera::View() const
{
    m4x4 matrix;
	gfxInterface.camera.getProperty(_id, CameraProperty::View, &matrix, sizeof(m4x4));
	return matrix;
}

m4x4 
Camera::Projection() const
{
	m4x4 matrix;
	gfxInterface.camera.getProperty(_id, CameraProperty::Projection, &matrix, sizeof(m4x4));
	return matrix;
}

m4x4 
Camera::InvProjection() const
{
    m4x4 matrix;
    gfxInterface.camera.getProperty(_id, CameraProperty::InverseProjection, &matrix, sizeof(m4x4));
    return matrix;
}

m4x4 
Camera::ViewProjection() const
{
    m4x4 matrix;
    gfxInterface.camera.getProperty(_id, CameraProperty::ViewProjection, &matrix, sizeof(m4x4));
    return matrix;
}

m4x4
Camera::InvViewProjection() const
{
    m4x4 matrix;
    gfxInterface.camera.getProperty(_id, CameraProperty::InverseViewProjection, &matrix, sizeof(m4x4));
    return matrix;
}

v3 
Camera::Up() const
{
    v3 vec;
    gfxInterface.camera.getProperty(_id, CameraProperty::UpVector, &vec, sizeof(v3));
    return vec;
}

f32 
Camera::NearZ() const
{
    f32 val;
	gfxInterface.camera.getProperty(_id, CameraProperty::NearZ, &val, sizeof(f32));
	return val;
}

f32 
Camera::FarZ() const
{
    f32 val;
    gfxInterface.camera.getProperty(_id, CameraProperty::FarZ, &val, sizeof(f32));
    return val;
}

v3 
Camera::AspectRatio()
{
    v3 val;
    gfxInterface.camera.getProperty(_id, CameraProperty::AspectRatio, &val, sizeof(v3));
    return val;
}

f32 
Camera::FieldOfView() const
{
    f32 val;
    gfxInterface.camera.getProperty(_id, CameraProperty::FieldOfView, &val, sizeof(f32));
    return val;
}

f32 
Camera::ViewWidth() const
{
    f32 val;
    gfxInterface.camera.getProperty(_id, CameraProperty::ViewWidth, &val, sizeof(f32));
    return val;
}

f32
Camera::ViewHeight() const
{
    f32 val;
    gfxInterface.camera.getProperty(_id, CameraProperty::ViewHeight, &val, sizeof(f32));
    return val;
}

Camera::Type 
Camera::ProjectionType()
{
    Camera::Type type;
	gfxInterface.camera.getProperty(_id, CameraProperty::ProjectionType, &type, sizeof(Camera::Type));
	return type;
}

id_t 
Camera::EntityID() const
{
	id_t id;
	gfxInterface.camera.getProperty(_id, CameraProperty::EntityId, &id, sizeof(id_t));  
	return id;
}

id_t
AddSubmesh(const u8*& data)
{
    return gfxInterface.resources.addSubmesh(data);
}

void
RemoveSubmesh(id_t id)
{
    gfxInterface.resources.removeSubmesh(id);
}

id_t 
AddTexture(const u8* const data)
{
    return gfxInterface.resources.addTexture(data);
}

void 
RemoveTexture(id_t id)
{
	gfxInterface.resources.removeTexture(id);
}

id_t 
AddMaterial(MaterialInitInfo info)
{
	return gfxInterface.resources.addMaterial(info);
}

void 
RemoveMaterial(id_t id)
{
	gfxInterface.resources.removeMaterial(id);
}

MaterialInitInfo GetMaterialReflection(id_t id)
{
    return gfxInterface.resources.getMaterialReflection(id);
}

id_t 
AddRenderItem(ecs::Entity entityID, id_t geometryContentID, u32 materialCount, const id_t* const materialIDs)
{
	return gfxInterface.resources.addRenderItem(entityID, geometryContentID, materialCount, materialIDs);
}

void RemoveRenderItem(id_t id)
{
	gfxInterface.resources.removeRenderItem(id);
}

}
