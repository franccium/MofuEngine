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
	explicit QueryView(std::vector<BlockPtr>&& blocks)
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
			: _blockOffset(blockOffset), _blockEnd(blockEnd)
		{
		}

		using returned_type = std::conditional_t<Writable, std::tuple<Entity, Component&...>, std::tuple<Entity, const Component&...>>;

		returned_type operator*() const
		{
			return { (*_blockOffset)->entities[_index], (*_blockOffset)->GetComponentArray<Component>()[_index]... };
		}

		Iterator& operator++()
		{
			++_index;
			if (_blockOffset != _blockEnd && _index == (*_blockOffset)->EntityCount)
			{
				++_blockOffset;
				_index = 0;
			}
			return *this;
		}

		bool operator==(const Iterator& o) const
		{
			return _blockOffset == o._blockOffset && (_blockEnd == o._blockEnd || _index == o._index);
		}

	private:
		BlockPtr* _blockOffset{ nullptr };
		BlockPtr* _blockEnd{ nullptr };
		u32 _index{ 0 };
	};

	Iterator begin() { return { _blocks.data(), _blocks.data() + _blocks.size() }; }
	Iterator end() { return { _blocks.data() + _blocks.size(), _blocks.data() + _blocks.size() }; }

private:
	std::vector<BlockPtr> _blocks{};
};

}