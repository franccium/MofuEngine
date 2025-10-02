#pragma once
#include "ECSCommon.h"
#include <bitset>

#if EDITOR_BUILD
#include "imgui.h"
#endif


namespace mofu::ecs::component {
struct Component {};
}

namespace mofu::ecs {
template<typename C>
concept IsComponent = std::derived_from<C, component::Component>;

using ComponentID = u32;

constexpr ComponentID MAX_COMPONENT_TYPES{ 256 };
using CetMask = std::bitset<MAX_COMPONENT_TYPES>;

}

namespace mofu::ecs::component {

template<IsComponent T>
void RenderComponentFields(T& component) {
    T::RenderFields(component);
}

}
