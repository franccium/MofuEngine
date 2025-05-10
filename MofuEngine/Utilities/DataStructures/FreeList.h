#pragma once
#include "CommonHeaders.h"
#include "Vector.h"

namespace mofu::util {
template<typename T>
class FreeList
{
static_assert(sizeof(T) >= sizeof(u32));
public:
	FreeList() = default;
	FreeList(u32 capacity)
	{
		_array.reserve(capacity);
	}
	~FreeList() { assert(_size == 0); }

	template<class... params>
	constexpr u32 add(params&&... p)
	{
		u32 id{ U32_INVALID_ID };
		if(_nextFreeIdx == U32_INVALID_ID)
		{
			id = (u32)_array.size();
			_array.emplace_back(std::forward<params>(p)...);
		}
		else
		{
			id = _nextFreeIdx;
			assert(id < _array.size() && "FreeList indexed out of bounds");
			_nextFreeIdx = *(const u32* const)std::addressof(_array[id]);
			new (std::addressof(_array[id])) T(std::forward<params>(p)...);
		}
		++_size;
		return id;
	}

	constexpr void remove(u32 id)
	{
		assert(id < _array.size() && "FreeList indexed out of bounds");
		T& item{ _array[id] };
		item.~T();
		*(u32* const)std::addressof(_array[id]) = _nextFreeIdx;
		_nextFreeIdx = id;
		--_size;
	}

	[[nodiscard]] constexpr u32 size() const { return _size; }
	[[nodiscard]] constexpr u32 capacity() const { return (u32)_array.size(); }
	[[nodiscard]] constexpr bool empty() const { return _size == 0; }

	[[nodiscard]] constexpr T& operator[](u32 id)
	{
		assert(id < _array.size() && "FreeList indexed out of bounds");
		return _array[id];
	}

	[[nodiscard]] constexpr const T& operator[](u32 id) const
	{
		assert(id < _array.size() && "FreeList indexed out of bounds");
		return _array[id];
	}

private:
	util::Vector<T, false> _array;
	u32 _nextFreeIdx{ U32_INVALID_ID };
	u32 _size{ 0 };
};
}