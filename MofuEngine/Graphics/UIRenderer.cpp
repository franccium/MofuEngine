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

ImTextureID
GetImTextureID(id_t textureID, u32 mipLevel, u32 format)
{
	return (ImTextureID)pi->ui.getImTextureIDIcon(textureID, mipLevel, format);
}

ImTextureID
GetImTextureID(id_t textureID, u32 arrayIndex, u32 mipLevel, u32 depthIndex, u32 format, bool isCubemap)
{
	return (ImTextureID)pi->ui.getImTextureID(textureID, arrayIndex, mipLevel, depthIndex, format, isCubemap);
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
