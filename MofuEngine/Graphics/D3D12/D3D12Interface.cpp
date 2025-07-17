#include "D3D12Interface.h"
#include "CommonHeaders.h"
#include "Graphics/GraphicsPlatformInterface.h"
#include "D3D12Core.h"
#include "D3D12Camera.h"
#include "D3D12Content.h"
#include "D3D12GUI.h"

namespace mofu::graphics::d3d12::content {
namespace geometry {
id_t AddSubmesh(const u8*& blob);
void RemoveSubmesh(id_t id);
}
namespace texture {
id_t AddTexture(const u8* const blob);
void RemoveTexture(id_t id);
}
namespace material {
id_t AddMaterial(const MaterialInitInfo& info);
void RemoveMaterial(id_t id);
}
}

namespace mofu::graphics::d3d12 {

void 
SetupPlatformInterface(PlatformInterface& pi)
{
	pi.platform = GraphicsPlatform::Direct3D12;

	pi.initialize = core::Initialize;
	pi.shutdown = core::Shutdown;

	pi.surface.create = core::CreateSurface;
	pi.surface.remove = core::RemoveSurface;
	pi.surface.render = core::RenderSurface;
	pi.surface.resize = core::ResizeSurface;
	pi.surface.width = core::SurfaceWidth;
	pi.surface.height = core::SurfaceHeight;

	pi.camera.create = camera::CreateCamera;
	pi.camera.remove = camera::RemoveCamera;
	pi.camera.setProperty = camera::SetProperty;
	pi.camera.getProperty = camera::GetProperty;

	pi.resources.addSubmesh = content::geometry::AddSubmesh;
	pi.resources.removeSubmesh = content::geometry::RemoveSubmesh;

	pi.resources.addTexture = content::texture::AddTexture;
	pi.resources.removeTexture = content::texture::RemoveTexture;	

	pi.resources.addMaterial = content::material::AddMaterial;	
	pi.resources.removeMaterial = content::material::RemoveMaterial;
	pi.resources.getMaterialReflection = content::material::GetMaterialReflection;

	pi.resources.addRenderItem = content::render_item::AddRenderItem;	
	pi.resources.removeRenderItem = content::render_item::RemoveRenderItem;

	//TODO: pi.ui.initialize = ui::Initialize;
	pi.ui.shutdown = ui::Shutdown;
	pi.ui.startNewFrame = ui::StartNewFrame;
	pi.ui.viewTexture = ui::ViewTextureAsImage;
	pi.ui.getImTextureID = ui::GetImTextureID;
	pi.ui.destroyViewTexture = ui::DestroyViewTexture;
	pi.ui.addIcon = content::texture::AddIcon;
}

}
