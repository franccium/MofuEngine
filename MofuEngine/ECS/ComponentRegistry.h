#pragma once
#include <array>
#include <tuple>
#include "ECSCommon.h"
#include "Component.h"
#include "Transform.h"

namespace mofu::ecs::scene {
    template<IsComponent C>
    void AddComponent(Entity e);
}

namespace mofu::ecs::component {

// FIXME: changing the order here would destroy the serialized components IDs
using ComponentTypes = std::tuple<
    LocalTransform,
    WorldTransform,
    Parent,
    Child,
    RenderMesh,
    RenderMaterial,
    Camera,
    Light,
    CullableLight,
    DirectionalLight,
    PointLight,
    SpotLight,
    NameComponent,
    PotentiallyVisible,
    CullableObject,
    DynamicObject,
    PathTraceable,
    Collider,
    StaticObject,
    DynamicObject
    //NOTE: dont put a , after the last item
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

constexpr std::array<u32, ComponentTypeCount> ComponentSizes {
    sizeof(LocalTransform),
    sizeof(WorldTransform),
    sizeof(Parent),
    sizeof(Child),
    sizeof(RenderMesh),
    sizeof(RenderMaterial),
    sizeof(Camera),
    sizeof(Light),
    sizeof(CullableLight),
    sizeof(DirectionalLight),
    sizeof(PointLight),
    sizeof(SpotLight),
    sizeof(NameComponent),
    sizeof(PotentiallyVisible),
    sizeof(CullableObject),
    sizeof(DynamicObject),
    sizeof(PathTraceable),
    sizeof(Collider),
    sizeof(StaticObject),
    sizeof(DynamicObject)
};

template<ComponentID ID>
inline constexpr u32 GetComponentSize() { static_assert(ID < ComponentTypeCount); return ComponentSizes[ID]; }

inline constexpr u32 GetComponentSize(ComponentID id) { assert(id < ComponentTypeCount); return ComponentSizes[id]; }

template<ComponentID ID>
void RenderOneComponent(void* raw)
{
    using T = ComponentTypeByID<ID>;
    RenderComponentFields(*static_cast<T*>(raw));
}


///////////////////////////////// DSIPLAYING COMPONENT DATA ///////////////////////////////////////////////////////////////

using RenderFunc = void(*)(void*);

template<u32... Is>
constexpr auto MakeRenderLUT(std::index_sequence<Is...>)
{
    return std::array<RenderFunc, sizeof...(Is)>{ &RenderOneComponent<Is>... };
}

constexpr auto RenderLUT{ MakeRenderLUT(std::make_index_sequence<ComponentTypeCount>{}) };

constexpr std::array<const char*, ComponentTypeCount> ComponentNames {
    "LocalTransform",
	"WorldTransform",
	"Parent",
	"Child",
	"RenderMesh",
	"RenderMaterial",
    "Camera",
    "Light",
    "CullableLight",
    "DirectionalLight",
    "PointLight",
    "SpotLight",
    "NameComponent",
    "PotentiallyVisible",
    "CullableObject",
	"DynamicObject",
	"PathTraceable",
	"Collider",
    "StaticObject",
    "DynamicObject"
};

//TODO: passing component ids as arguments might be a better idea
template<IsComponent C>
void AddComponent(Entity e)
{
    // if (scene::HasComponent<C>(e)) return;
    scene::AddComponent<C>(e);
}

using AddFunc = void(*)(Entity);

template <std::size_t... Is>
constexpr auto MakeAddLUT(std::index_sequence<Is...>)
{
    return std::array<AddFunc, sizeof...(Is)>{{ &AddComponent<ComponentTypeByID<Is>>... }};
}

inline constexpr auto AddLUT = MakeAddLUT(std::make_index_sequence<ComponentTypeCount>{});


///////////////////////////////// SERIALIZATION ///////////////////////////////////////////////////////////////

//namespace detail {
//template<typename T, typename = void>
//struct IsYamlSerializable : std::false_type {};
//
//// checks whether an appropriate overload for YAML exists, if it does, it assumes the type to be serializable
//template<typename T>
//struct IsYamlSerializable<T, std::void_t<decltype(operator<<(std::declval<YAML::Emitter&>(), std::declval<const T&>()))>> : std::true_type {};
//}
//
//template<typename T>
//constexpr bool IsYamlSerializable_v = detail::IsYamlSerializable<T>::value;

using SerializeFunc = void(*)(YAML::Emitter&, const u8* const);

template<ComponentID ID>
void SerializeComponent(YAML::Emitter& out, const u8* const componentData) 
{
    using T = ComponentTypeByID<ID>;

    if constexpr (IsYamlSerializable<T>) 
    {
        out << *reinterpret_cast<const T*>(componentData);
    }
    else 
    {
        out << YAML::Null;
    }
}

template<u32... Is>
constexpr auto MakeSerializeLUT(std::index_sequence<Is...>)
{
    return std::array<SerializeFunc, sizeof...(Is)>{ &SerializeComponent<Is>... };
}

inline constexpr auto SerializeLUT = MakeSerializeLUT(std::make_index_sequence<ComponentTypeCount>{});


using DeserializeFunc = void(*)(const YAML::Node&, u8*);

template<ComponentID ID>
void DeserializeComponent(const YAML::Node& node, u8* data) 
{
    using T = ComponentTypeByID<ID>;

    if constexpr (IsYamlDeserializable<T>)
    {
        //*reinterpret_cast<T*>(data) = node.as<T>();
        node >> *reinterpret_cast<T*>(data);
    }
    else
    {
        new (reinterpret_cast<T*>(data)) T{};
    }
}

template<u32... Is>
constexpr auto MakeDeserializeLUT(std::index_sequence<Is...>) 
{
    return std::array<DeserializeFunc, sizeof...(Is)>{ &DeserializeComponent<Is>... };
}

constexpr auto DeserializeLUT = MakeDeserializeLUT(std::make_index_sequence<ComponentTypeCount>{});

//NOTE: not all components are compile-time constructible (e.g. Collider where JPH::BodyID is not) so cant use apply max
template<typename... Ts>
constexpr u32 MaxSize()
{
    u32 max{ 0 };
    ((max = max < sizeof(Ts) ? sizeof(Ts) : max), ...);
    return max;
}

template<typename Tuple>
struct BiggestComponentSize;

template<typename... Ts>
struct BiggestComponentSize<std::tuple<Ts...>>
{
    static constexpr size_t value = MaxSize<Ts...>();
};

constexpr u32 GET_BIGGEST_COMPONENT_SIZE =
BiggestComponentSize<ComponentTypes>::value;
}