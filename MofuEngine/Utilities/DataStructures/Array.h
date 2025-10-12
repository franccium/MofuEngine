#pragma once
#include "CommonHeaders.h"
#include "../TypeUtils.h"

namespace mofu {
template<typename T>
class Array
{
public:
	constexpr Array() = default;
	constexpr explicit Array(u64 elementCount)
	{
		Initialize(elementCount);
	}

	constexpr explicit Array(u64 capacity, const T& value)
	{
		Initialize(capacity, value);
	}

	constexpr Array(const Array& o)
	{
		*this = o;
	}

	constexpr Array(Array&& o) : _size{ o._size }, _data{ o._data }
	{
		o.Reset();
	}

	constexpr Array& operator=(const Array& o)
	{
		assert(this != std::addressof(o));
		if (this != std::addressof(o))
		{
			Resize(o._size);
			memcpy(_data, o._data, o._size);
		}
		return *this;
	}

	constexpr Array& operator=(Array&& o)
	{
		assert(this != std::addressof(o));
		if (this != std::addressof(o))
		{
			Destroy();
			Move(o);
		}
		return *this;
	}

	~Array() { Destroy(); }

	constexpr void Initialize(u64 elementCount)
	{
		//static_assert(std::is_default_constructible<T>::value, "Type must be default-contructible");
		Destroy();
		_size = elementCount;
		if(elementCount)
			_data = new T[elementCount];
	}

	constexpr void Initialize(u64 elementCount, const T& value)
	{
		static_assert(std::is_copy_assignable<T>::value, "Type must be copy-assignable");

		Initialize(elementCount);
		for (u32 i{ 0 }; i < _size; ++i) _data[i] = value;
	}

	void Resize(u64 newElementCount)
	{
		assert(newElementCount);
		if (newElementCount == _size) return;
		T* data{ new T[newElementCount] };
		for (u32 i{ 0 }; i < _size; ++i) data[i] = _data[i];
		Destroy();
		_size = newElementCount;
		_data = data;
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

	[[nodiscard]] constexpr T& operator[](u64 index)
	{
		assert(_data && index < _size && "Array indexed out of bounds");
		return _data[index];
	}

	[[nodiscard]] constexpr const T& operator[](u64 index) const
	{
		assert(_data && index < _size && "Array indexed out of bounds");
		return _data[index];
	}

private:
	constexpr void Destroy()
	{
		_size = 0;
		if (_data)
		{
			delete[] _data;
			_data = nullptr;
		}
	}

	constexpr void Reset()
	{
		_size = 0;
		_data = nullptr;
	}

	constexpr void Move(Array& o)
	{
		_size = o._size;
		_data = o._data;
		o.Reset();
	}

	u64 _size{ 0 };
	T* _data{ nullptr };
};

}