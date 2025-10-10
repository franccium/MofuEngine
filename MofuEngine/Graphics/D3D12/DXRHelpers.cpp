#include "DXRHelpers.h"

namespace mofu::graphics::d3d12::rt {
namespace {
static constexpr u64 MAX_SUBOBJECT_DESC_SIZE{ sizeof(D3D12_HIT_GROUP_DESC) };

} // anonymous namespace

void 
RTStateObjectStream::Reserve(u32 subobjectCount)
{
	assert(subobjectCount > 0);

	SubobjectMax = subobjectCount;
	Subobjects.Initialize(SubobjectMax);
	const u64 dataSize{ SubobjectMax * MAX_SUBOBJECT_DESC_SIZE };
	Data.Initialize(dataSize, 0);
}

const D3D12_STATE_SUBOBJECT* 
RTStateObjectStream::AddSubobject(const void* subobjectDesc, u64 descSize, D3D12_STATE_SUBOBJECT_TYPE type)
{
	assert(subobjectDesc);
	assert(descSize > 0 && descSize <= MAX_SUBOBJECT_DESC_SIZE);
	assert(SubobjectCount < SubobjectMax);

	const u64 subobjectOffset{ SubobjectCount * MAX_SUBOBJECT_DESC_SIZE };
	memcpy(Data.data() + subobjectOffset, subobjectDesc, descSize);
	D3D12_STATE_SUBOBJECT& subObject{ Subobjects[SubobjectCount] };
	subObject.Type = type;
	subObject.pDesc = Data.data() + subobjectOffset;
	++SubobjectCount;

	return &subObject;
}

ID3D12StateObject* 
RTStateObjectStream::Create(D3D12_STATE_OBJECT_TYPE type)
{
	D3D12_STATE_OBJECT_DESC desc{};
	desc.Type = type;
	desc.NumSubobjects = SubobjectCount;
	desc.pSubobjects = Subobjects.data();

	ID3D12StateObject* obj{ nullptr };
	DXCall(core::Device()->CreateStateObject(&desc, IID_PPV_ARGS(&obj)));
	return obj;
}

}
