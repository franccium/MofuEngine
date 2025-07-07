#pragma once
#include "ECSCommon.h"
#include "ECSCore.h"

namespace mofu::ecs {
//TODO: might add bool WithEntities here but idk
template<bool Writable, typename... Component>
class QueryView
{
	using BlockPtr = EntityBlock*;

public:
	explicit QueryView(std::vector<BlockPtr>& blocks)
		: _blocks(std::move(blocks))
	{
	}

	class Iterator
	{
		// figure out how to handle pointers
	public:
		// TODO: actual read-only views

		//TODO: rethink my first idea 
		Iterator(BlockPtr* blockOffset, BlockPtr* blockEnd)
			: _currentBlock(blockOffset), _lastBlock(blockEnd)
		{
		}

		using returned_type = std::conditional_t<Writable, std::tuple<Entity, Component&...>, std::tuple<Entity, const Component&...>>;

		returned_type operator*() const
		{
			return { (*_currentBlock)->Entities[_index], (*_currentBlock)->GetComponentArray<Component>()[_index]... };
		}

		Iterator& operator++()
		{
			++_index;
			while (_currentBlock != _lastBlock && _index == (*_currentBlock)->EntityCount)
			{
				++_currentBlock;
				_index = 0;
			}
			return *this;
		}

		bool operator==(const Iterator& o) const
		{
			return _currentBlock == o._currentBlock && (_lastBlock == o._lastBlock || _index == o._index);
		}

	private:
		BlockPtr* _currentBlock{ nullptr };
		BlockPtr* _lastBlock{ nullptr };
		u32 _index{ 0 };
	};

	Iterator begin() { return { _blocks.data(), _blocks.data() + _blocks.size() }; }
	Iterator end() { return { _blocks.data() + _blocks.size(), _blocks.data() + _blocks.size() }; }

private:
	std::vector<BlockPtr> _blocks{};
};

}