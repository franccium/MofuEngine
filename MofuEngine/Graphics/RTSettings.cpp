#include "RTSettings.h"

namespace mofu::graphics::rt::settings {

v3 SunIrradiance;
v3 SunDirection;
v3 SunColor;
f32 SunAngularRadius;

void 
Initialize()
{
    SunAngularRadius = 1.f;
    SunIrradiance = v3{ 1.f, 1.f, 1.f };
    SunDirection = v3{ 0.3f, 0.92f, 0.1f };
	SunColor = v3{ 1.f, 0.956f, 0.84f };
}

u32
GetSceneMeshCount()
{//TODO:
    return 1;
}

}