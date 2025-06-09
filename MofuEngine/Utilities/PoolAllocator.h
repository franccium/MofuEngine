#pragma once
#include "CommonHeaders.h"

namespace mofu::memory {
template<typename T>
class PoolAllocator
{
public:
	T* Allocate() 
	{
		if (_freeList.empty())
		{
			return static_cast<T*>(std::malloc(sizeof(T)));
		}

		T* ptr{ _freeList.back() };
		_freeList.pop_back();
		return ptr;
	}

	void Deallocate(T* ptr)
	{
		ptr->~T();
		_freeList.push_back(ptr);
	}

	~PoolAllocator()
	{
		for (T* ptr : _freeList)
		{
			std::free(ptr);
		}
	}

private:
	Vec<T*> _freeList;
};

}