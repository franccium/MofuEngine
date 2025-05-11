#pragma once

#include "D3D12CommonHeaders.h"
#include "D3D12Core.h"

namespace mofu::graphics::d3d12 {
class DescriptorHeap
{
public:
	explicit DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type) : _type{ type } {}
	DISABLE_COPY_AND_MOVE(DescriptorHeap);
	~DescriptorHeap() { assert(!_heap); }


private:
	D3D12_DESCRIPTOR_HEAP_TYPE _type;
	ID3D12DescriptorHeap* _heap{ nullptr };

};
}