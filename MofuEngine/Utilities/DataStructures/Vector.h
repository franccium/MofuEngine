#pragma once
#include "CommonHeaders.h"
#include "../TypeUtils.h"

namespace mofu::util {
// destructItems - call the destructor for each element when clearing/destroying the vector
template<typename T, bool destructItems = true>
class Vector
{
public:
	constexpr Vector() = default;
	constexpr explicit Vector(u64 capacity)
	{
		resize(capacity);
	}

	constexpr explicit Vector(u64 capacity, const T& value)
	{
		resize(capacity, value);
	}

	constexpr Vector(const Vector& o)
	{
		*this = o;
	}

	constexpr Vector(Vector&& o) : _capacity{ o._capacity }, _size{ o._size }, _data{ o._data }
	{
		o.reset();
	}

	constexpr Vector& operator=(const Vector& o)
	{
		assert(this != std::addressof(o));
		if (this != std::addressof(o))
		{
			clear();
			reserve(o._size);
			for (const auto& item : o)
			{
				emplace_back(item);
			}
			assert(_size == o._size);
		}
		return *this;
	}

	constexpr Vector& operator=(Vector&& o)
	{
		assert(this != std::addressof(o));
		if (this != std::addressof(o))
		{
			destroy();
			move(o);
		}
		return *this;
	}

	~Vector() { destroy(); }

	constexpr void push_back(const T& value)
	{
		emplace_back(value);
	}

	constexpr void push_back(T&& value)
	{
		emplace_back(std::move(value));
	}

	template <typename... params>
	constexpr decltype(auto) emplace_back(params&&... p)
	{
		if (_size == _capacity)
		{
			// reserve 50% more capacity
			reserve(((_capacity + 1) * 3) >> 1);
		}
		assert(_capacity > _size);

		T* const item{ new(std::addressof(_data[_size])) T(std::forward<params>(p)...) };
		++_size;
		return item;
	}

	constexpr void resize(u64 newSize)
	{
		static_assert(std::is_default_constructible<T>::value, "Type must be default-contructible");

		if (newSize > _size)
		{
			reserve(newSize);
			while (_size < newSize)
			{
				emplace_back();
			}
		}
		else if (newSize < _size)
		{
			if constexpr (destructItems)
			{
				destruct_range(newSize, _size);
			}
			_size = newSize;
		}
		assert(_size == newSize);
	}

	constexpr void resize(u64 newSize, const T& value)
	{
		static_assert(std::is_default_constructible<T>::value, "Type must be default-contructible");

		if (newSize > _size)
		{
			reserve(newSize);
			while (_size < newSize)
			{
				emplace_back(value);
			}
		}
		else if (newSize < _size)
		{
			if constexpr (destructItems)
			{
				destruct_range(newSize, _size);
			}
			_size = newSize;
		}
		assert(_size == newSize);
	}

	constexpr void reserve(u64 newCapacity)
	{
		if (newCapacity > _capacity)
		{
			void* newBuffer{ realloc(_data, newCapacity * sizeof(T)) };
			assert(newBuffer);
			if (newBuffer)
			{
				_data = static_cast<T*>(newBuffer);
				_capacity = newCapacity;
			}
		}
	}

	constexpr T* const erase(u64 index)
	{
		assert(index <= _size && _data);
		return erase(std::addressof(_data[index]));
	}

	constexpr T* const erase(T* const item)
	{
		assert(_data && item >= begin() && item < end());
		if constexpr (destructItems) item->~T();
		--_size;
		if (item < end())
		{
			memcpy(item, item + 1, (end() - item) * sizeof(T));
		}
		return item;
	}

	// removes the item at index and copies the last item into that index
	constexpr T* const erase_unordered(u64 index)
	{
		assert(index <= _size && _data);
		return erase_unordered(std::addressof(_data[index]));
	}

	// removes the item and copies the last item into that item's index
	constexpr T* const erase_unordered(T* const item)
	{
		assert(_data && item >= begin() && item < end());
		if constexpr (destructItems) item->~T();
		--_size;
		if (item < end())
		{
			memcpy(item, end(), sizeof(T));
		}
		return item;
	}

	constexpr void swap(Vector& o)
	{
		if (this != std::addressof(o))
		{
			auto temp{ std::move(0) };
			o.move(*this);
			move(temp);
		}
	}

	constexpr void clear()
	{
		if constexpr (destructItems)
		{
			destruct_range(0, _size);
		}
		_size = 0;
	}

	[[nodiscard]] constexpr T* data() { return _data; }
	[[nodiscard]] constexpr const T* data() const { return _data; }

	[[nodiscard]] constexpr T& front() { assert(_data && _size != 0); return _data[0]; }
	[[nodiscard]] constexpr const T& front() const { assert(_data && _size != 0); return _data[0]; }
	[[nodiscard]] constexpr T& back() { assert(_data && _size != 0); return _data[_size - 1]; }
	[[nodiscard]] constexpr const T& back() const { assert(_data && _size != 0); return _data[_size - 1]; }

	[[nodiscard]] constexpr T* begin() { return std::addressof(_data[0]); }
	[[nodiscard]] constexpr const T* begin() const { return std::addressof(_data[0]); }
	[[nodiscard]] constexpr T* end() { assert(!(_data == nullptr && _size > 0)); return std::addressof(_data[_size]); }
	[[nodiscard]] constexpr const T* end() const { assert(!(_data == nullptr && _size > 0)); return std::addressof(_data[_size]); }

	[[nodiscard]] constexpr bool empty() const { return _size == 0; }
	[[nodiscard]] constexpr u64 size() const { return _size; }
	[[nodiscard]] constexpr u64 capacity() const { return _capacity; }

	[[nodiscard]] constexpr T& operator[](u64 index)
	{
		assert(_data && index < _size && "Vector indexed out of bounds");
		return _data[index];
	}

	[[nodiscard]] constexpr const T& operator[](u64 index) const
	{
		assert(_data && index < _size && "Vector indexed out of bounds");
		return _data[index];
	}

	template<typename it, typename = std::enable_if_t<_is_iterator_v<it>>>
	constexpr explicit Vector(it first, it last)
	{
		for (; first != last; ++first)
		{
			emplace_back(*first);
		}
	}

private:
	constexpr void destroy()
	{
		assert([&] {return _capacity ? _data != nullptr : _data == nullptr; } ());
		clear();
		_capacity = 0;
		if (_data) free(_data);
		_data = nullptr;
	}

	constexpr void destruct_range(u64 first, u64 last)
	{
		assert(destructItems);
		assert(first <= _size && last <= _size && first <= last);
		if (_data)
		{
			for (; first != last; ++first)
			{
				_data[first].~T();
			}
		}
	}

	constexpr void reset()
	{
		_capacity = 0;
		_size = 0;
		_data = nullptr;
	}

	constexpr void move(Vector& o)
	{
		_capacity = o._capacity;
		_size = o._size;
		_data = o._data;
		o.reset();
	}

	u64 _capacity{ 0 };
	u64 _size{ 0 };
	T* _data{ nullptr };
};

}