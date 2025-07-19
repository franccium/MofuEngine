#pragma once
#include "CommonHeaders.h"
#include "yaml-cpp/yaml.h"
#include "Utilities/MathTypes.h"

using namespace mofu;

namespace mofu {
// checks whether an appropriate overload for YAML exists, if it does, it assumes the type to be serializable
template<typename T>
concept IsYamlSerializable = requires(YAML::Emitter& out, const T& t) 
{
    { out << t } -> std::same_as<YAML::Emitter&>;
};
template<typename T>
concept IsYamlDeserializable = requires(const YAML::Node& node, T& t)
{
    { node >> t } -> std::same_as<bool>;
};
}

namespace YAML {
inline Emitter& operator<<(Emitter& out, const v2& v) 
{
    out << Flow << BeginSeq << v.x << v.y << EndSeq;
    return out;
}

inline Emitter& operator<<(Emitter& out, const v3& v) 
{
    out << Flow << BeginSeq << v.x << v.y << v.z << EndSeq;
    return out;
}

inline Emitter& operator<<(Emitter& out, const v4& v) 
{
    out << Flow << BeginSeq << v.x << v.y << v.z << v.w << EndSeq;
    return out;
}

inline Emitter& operator<<(Emitter& out, const m4x4& m)
{
    out << Flow << BeginSeq << m.m << EndSeq;
    return out;
}

inline bool operator>>(const Node& node, v2& v)
{
    v.x = node[0].as<f32>();
    v.y = node[1].as<f32>();
    return true;
}

inline bool operator>>(const Node& node, v3& v)
{
    v.x = node[0].as<f32>();
    v.y = node[1].as<f32>();
    v.z = node[2].as<f32>();
    return true;
}

inline bool operator>>(const Node& node, v4& v)
{
    v.x = node[0].as<f32>();
    v.y = node[1].as<f32>();
    v.z = node[2].as<f32>();
    v.w = node[3].as<f32>();
    return true;
}

}