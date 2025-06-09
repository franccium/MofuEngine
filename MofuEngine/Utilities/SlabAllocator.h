#pragma once
#include "CommonHeaders.h"
#include <cstdlib>
namespace mofu::memory {
template<size_t SlabSize, size_t Alignment>
class SlabAllocator
{
public:
	void* Allocate()
	{
		if (_freeSlabs.empty())
		{
			void* slab{ _aligned_malloc(SlabSize, Alignment) };
			assert(slab);
			return slab;
		}

		void* slab{ _freeSlabs.back() };
		_freeSlabs.pop_back();
		return slab;
	}

	void Deallocate(void* slab)
	{
		_freeSlabs.push_back(slab);
	}

	~SlabAllocator()
	{
		for(void* slab : _slabs)
		{
			_aligned_free(slab);
		}
	}
private:
	Vec<void*> _slabs;
	Vec<void*> _freeSlabs;
};
}