#pragma once

template<typename T>
struct ComponentID;

namespace mofu::ecs::component {
struct Component
{

};

struct TestComponent
{
    v4 data{};
    u32 data2{};
};

struct TestComponent2
{
    v4 data{};
    u32 data2{};
};

struct TestComponent3
{
    v4 data{};
    u32 data2{};
};

struct TestComponent4
{
    v4 data{};
    u32 data2{};
};

struct TestComponent5
{
    v4 data{};
    u32 data2{};
};

//template<typename T>
//struct ComponentTypeID
//{
//	//constinit?
//
//	//consteval?
//	static constexpr u32 value = typeid(T).hash_code();
//};

class ComponentIDGenerator {
public:
    template<typename T>
    static u64 GetID() {
        static const u64 id = counter++;
        return id;
    }
private:
    static inline u64 counter = 0;
};
 
}
