#pragma once
#include "ECSCommon.h"
#include "Component.h"
#include "Transform.h"
#include <array>
#include <tuple>

namespace mofu::ecs::component {

using ComponentTypes = std::tuple<
    LocalTransform,
    WorldTransform,
    Parent,
    RenderMesh,
    RenderMaterial,
    TestComponent,
    TestComponent2,
    TestComponent3,
    TestComponent4,
    TestComponent5
>;
constexpr u32 ComponentTypeCount{ std::tuple_size_v<ComponentTypes> };

// Find the corresponding component in the tuple recursively
template<typename C, typename Tuple>
struct ComponentIdx;
template<typename C, typename... Types>
struct ComponentIdx<C, std::tuple<C, Types...>> : std::integral_constant<u32, 0> {};
template<typename C, typename H, typename... Types>
struct ComponentIdx<C, std::tuple<H, Types...>> : std::integral_constant<u32, 1 + ComponentIdx<C, std::tuple<Types...>>::value> {};

template<typename C>
inline constexpr ComponentID ID = [] { return ComponentIdx<C, ComponentTypes>::value; }();

template<ComponentID ID>
using ComponentTypeByID = std::tuple_element_t<ID, ComponentTypes>;

constexpr std::array<ComponentID, ComponentTypeCount> ComponentSizes {
    sizeof(LocalTransform),
    sizeof(WorldTransform),
    sizeof(Parent),
    sizeof(RenderMesh),
    sizeof(RenderMaterial),
    sizeof(TestComponent),
    sizeof(TestComponent2),
    sizeof(TestComponent3),
    sizeof(TestComponent4),
    sizeof(TestComponent5),
};

template<ComponentID ID>
inline constexpr u32 GetComponentSize() { assert(ID < ComponentTypeCount); return ComponentSizes[ID]; }

inline constexpr u32 GetComponentSize(ComponentID id) { assert(id < ComponentTypeCount); return ComponentSizes[id]; }

template<ComponentID ID>
void RenderOneComponent(void* raw)
{
    using T = ComponentTypeByID<ID>;
    RenderComponentFields(*static_cast<T*>(raw));
}


using RenderFn = void(*)(void*);

template<std::size_t... Is>
constexpr auto MakeRenderLUT(std::index_sequence<Is...>)
{
    return std::array<RenderFn, sizeof...(Is)>{ &RenderOneComponent<Is>... };
}

constexpr auto RenderLUT =
MakeRenderLUT(std::make_index_sequence<ComponentTypeCount>{});

}