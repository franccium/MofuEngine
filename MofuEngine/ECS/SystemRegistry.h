#pragma once
#include "ECSCommon.h"
#include "EngineAPI/ECS/SystemAPI.h"
#include <array>
#include <functional>

/*
* contains all registered systems
* these can be grouped together
* and ordered within the group
*/

namespace mofu::ecs::system {

struct SystemEntry
{
	u32 GroupOrder{ 0 };
	SystemGroup::Group Group{ SystemGroup::Update };
	//std::unique_ptr<System> SystemPtr;
	std::function<void(SystemUpdateData)> SystemUpdate;
};

template<typename T>
concept IsSystem = std::is_base_of_v<System<T>, T>;

class SystemRegistry
{
public:
	SystemRegistry() = default;
	~SystemRegistry() = default;
	DISABLE_COPY_AND_MOVE(SystemRegistry);

	static SystemRegistry& Instance()
	{
		static SystemRegistry instance;
		return instance;
	}

	// TODO: what should be sent for updates 
	void UpdateSystems(SystemGroup::Group group, const SystemUpdateData data)
	{
		for (auto& systemEntry : _systems[group])
		{
			//assert(systemEntry.SystemPtr);
			//systemEntry.SystemPtr->Update(data);
			systemEntry.SystemUpdate(data);
		}
	}

	template<IsSystem T>
	void RegisterSystem(SystemGroup::Group group = SystemGroup::Update, u32 order = -1)
	{
		// TODO: make sure to do this only if there is no update going on
		if (order < 0)
		{
			//_systems[group].emplace_back(_systems[group].size(), group, std::make_unique<T>());
			_systems[group].emplace_back((u32)_systems[group].size(), group, [](SystemUpdateData data) {T system{}; system.Update(data); });
		}
		else
		{
			if (order > (u32)_systems[group].size()) order = (u32)_systems[group].size();
			// insert at the specified order and shift the rest
			//_systems[group].emplace_back();
			//std::shift_right(_systems[group].begin() + order, _systems[group].end(), 1);
			//std::for_each(_systems[group].begin() + order + 1, _systems[group].end(), [&](SystemEntry& entry) { entry.GroupOrder++; });
			
			_systems[group].insert(_systems[group].begin() + order, SystemEntry{ order, group, [](SystemUpdateData data) {T system{}; system.Update(data); } });
			for (u32 i{ order + 1 }; i < _systems[group].size(); ++i)
			{
				_systems[group][i].GroupOrder++;
			}

			//_systems[group][order] = SystemEntry{ order, std::make_unique<T>() };
			new(&_systems[group][order]) SystemEntry{ order, group, [](SystemUpdateData data) {T system{}; system.Update(data); } };
		}
	}

private:
	// systems' order index -- ptr to System
	//using SystemEntry = std::pair<u32, std::unique_ptr<System>>;
	std::array<std::vector<SystemEntry>, SystemGroup::Count> _systems;
};

}


namespace mofu::ecs::system::detail
{
	template<IsSystem T>
	struct RegisterScriptCall
	{
		RegisterScriptCall(SystemGroup::Group g, u32 o)
		{
			SystemRegistry::Instance().RegisterSystem<T>(g, o);
		}
	};
}