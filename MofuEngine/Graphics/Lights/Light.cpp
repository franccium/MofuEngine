#include "Light.h"

namespace mofu::graphics::light {
namespace {

util::FreeList<LightSet> lightSets{};

} // anonymous namespace

LightSet& 
GetLightSet(u32 lightSetKey)
{
	assert(lightSetKey < lightSets.size());
	return lightSets[lightSetKey];
}

void 
CreateLightSet(u32 lightSetKey)
{
}

void 
RemoveLightSet(u32 lightSetKey)
{
}

void 
AddLightToLightSet(ecs::Entity lightEntity)
{
}
}