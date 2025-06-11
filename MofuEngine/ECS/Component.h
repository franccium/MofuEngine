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

struct TestComponent : Component
{
    v4 data{};
    u32 data2{};

    static void RenderFields(TestComponent& c)
    {
		ImGui::InputFloat4("Data", &c.data.x);
		ImGui::InputScalar("Data2", ImGuiDataType_U32, &c.data2);
    }
};

struct TestComponent2 : Component
{
    v4 data{};
    u32 data2{};

    static void RenderFields(TestComponent2& c)
    {
        ImGui::InputFloat4("Data", &c.data.x);
        ImGui::InputScalar("Data2", ImGuiDataType_U32, &c.data2);
    }
};

struct TestComponent3 : Component
{
    v4 data{};
    u32 data2{};

    static void RenderFields(TestComponent3& c)
    {
        ImGui::InputFloat4("Data", &c.data.x);
        ImGui::InputScalar("Data2", ImGuiDataType_U32, &c.data2);
    }
};

struct TestComponent4 : Component
{
    v4 data{};
    u32 data2{};

    static void RenderFields(TestComponent4& c)
    {
        ImGui::InputFloat4("Data", &c.data.x);
        ImGui::InputScalar("Data2", ImGuiDataType_U32, &c.data2);
    }
};

struct TestComponent5 : Component
{
    v4 data{};
    u32 data2{};

    static void RenderFields(TestComponent5& c)
    {
        ImGui::InputFloat4("Data", &c.data.x);
        ImGui::InputScalar("Data2", ImGuiDataType_U32, &c.data2);
    }
};

template<typename T>
void RenderComponentFields(T& component) {
    T::RenderFields(component);
}

}
