#pragma once
#include "ECS/ECSCommon.h"
//namespace mofu::ecs {
//
//#define REGISTER_SYSTEM(System, Group, Order) \
//	mofu::ecs::system::SystemRegistry::Instance().RegisterSystem<System>(Group, Order);
//
//}

namespace mofu::ecs::system {

struct SystemUpdateData
{
	f32 DeltaTime{ 0.0f };
};

struct SystemGroup
{
	enum Group : u32
	{
		Initial,
		PreUpdate,
		Update,
		PostUpdate,
		Final,
		Count
	};
};

template<typename DerivedSystem>
struct System
{
	void Update(const SystemUpdateData data)
	{
		static_cast<DerivedSystem*>(this)->Update(data);
	}
};

}
#include "ECS/SystemRegistry.h"


#define REGISTER_SYSTEM(Type, Group, Order)                                       \
	static ::mofu::ecs::system::detail::AutoRegistrar<Type>                      \
		_reg_##Type##_##__LINE__ { (Group), (Order) }
