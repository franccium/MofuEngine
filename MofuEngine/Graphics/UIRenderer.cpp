#include "UIRenderer.h"
#include "GraphicsPlatformInterface.h"

namespace mofu::graphics::ui {
namespace {
const PlatformInterface* pi{ nullptr };

}

bool 
Initialize(const PlatformInterface* const platformInterface)
{
	pi = platformInterface;
	//TODO: pi->ui.initialize();
	return true;
}

void 
Shutdown()
{
	pi->ui.shutdown();
}

void
StartNewFrame()
{
	pi->ui.startNewFrame();
}

void
ViewTexture(id_t textureID)
{
	pi->ui.viewTexture(textureID);
}

u64
GetImTextureID(id_t textureID, u32 mipLevel, u32 format)
{
	return pi->ui.getImTextureID(textureID, mipLevel, format);
}

id_t 
AddIcon(const u8* const blob)
{
	return pi->ui.addIcon(blob);
}

void
DestroyViewTexture(id_t textureID)
{
	pi->ui.destroyViewTexture(textureID);
}

}
