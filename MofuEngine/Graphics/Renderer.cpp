#include "Renderer.h"
#include "GraphicsPlatformInterface.h"
#include "D3D12/D3D12Interface.h"

namespace mofu::graphics {
namespace {
constexpr const char* ENGINE_SHADERS_PATHS[]{
    ".\\shaders\\d3d12\\shaders.bin"
};

PlatformInterface gfxInterface;

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

bool
Initialize(GraphicsPlatform platform)
{
    return SetupPlatformInterface(platform) && gfxInterface.initialize();
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

}
