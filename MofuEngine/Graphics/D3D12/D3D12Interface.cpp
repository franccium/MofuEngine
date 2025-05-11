#include "D3D12Interface.h"
#include "CommonHeaders.h"
#include "Graphics\GraphicsPlatformInterface.h"
#include "D3D12Core.h"

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
}

}
