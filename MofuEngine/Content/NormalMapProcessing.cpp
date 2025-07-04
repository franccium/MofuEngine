#include "CommonHeaders.h"
#include "Content/ResourceCreation.h"
#include "Utilities/IOStream.h"
#include <DirectXTex.h>

using namespace DirectX;

namespace mofu::content {
namespace {


} // anonymous namespace

bool
IsNormalMap(const Image* const image)
{
	const DXGI_FORMAT image_format{ image->format };
	if (BitsPerPixel(image_format) != 32 || BitsPerColor(image_format) != 8) return false;
	//TODO:

	return false;
}
}