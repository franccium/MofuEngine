#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::rt {
struct RTStateObjectStream
{
    Array<u8> Data;
    Array<D3D12_STATE_SUBOBJECT> Subobjects;
    u32 SubobjectCount = 0;
    u32 SubobjectMax = 0;

    explicit RTStateObjectStream(u32 subobjectCount)
    {
        Reserve(subobjectCount);
    }

    void Reserve(u32 subobjectCount);
    const D3D12_STATE_SUBOBJECT* AddSubobject(const void* subobjectDesc, u64 descSize, D3D12_STATE_SUBOBJECT_TYPE type);
    ID3D12StateObject* Create(D3D12_STATE_OBJECT_TYPE type);

    const D3D12_STATE_SUBOBJECT* AddSubobject(const D3D12_STATE_OBJECT_CONFIG& subobjectDesc)
    {
        return AddSubobject(&subobjectDesc, sizeof(subobjectDesc), D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG);
    }

    const D3D12_STATE_SUBOBJECT* AddSubobject(const D3D12_GLOBAL_ROOT_SIGNATURE& subobjectDesc)
    {
        return AddSubobject(&subobjectDesc, sizeof(subobjectDesc), D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE);
    }

    const D3D12_STATE_SUBOBJECT* AddSubobject(const D3D12_LOCAL_ROOT_SIGNATURE& subobjectDesc)
    {
        return AddSubobject(&subobjectDesc, sizeof(subobjectDesc), D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE);
    }

    const D3D12_STATE_SUBOBJECT* AddSubobject(const D3D12_NODE_MASK& subobjectDesc)
    {
        return AddSubobject(&subobjectDesc, sizeof(subobjectDesc), D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK);
    }

    const D3D12_STATE_SUBOBJECT* AddSubobject(const D3D12_DXIL_LIBRARY_DESC& subobjectDesc)
    {
        return AddSubobject(&subobjectDesc, sizeof(subobjectDesc), D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY);
    }

    const D3D12_STATE_SUBOBJECT* AddSubobject(const D3D12_EXISTING_COLLECTION_DESC& subobjectDesc)
    {
        return AddSubobject(&subobjectDesc, sizeof(subobjectDesc), D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION);
    }

    const D3D12_STATE_SUBOBJECT* AddSubobject(const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION& subobjectDesc)
    {
        return AddSubobject(&subobjectDesc, sizeof(subobjectDesc), D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
    }

    const D3D12_STATE_SUBOBJECT* AddSubobject(const D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION& subobjectDesc)
    {
        return AddSubobject(&subobjectDesc, sizeof(subobjectDesc), D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
    }

    const D3D12_STATE_SUBOBJECT* AddSubobject(const D3D12_RAYTRACING_SHADER_CONFIG& subobjectDesc)
    {
        return AddSubobject(&subobjectDesc, sizeof(subobjectDesc), D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG);
    }

    const D3D12_STATE_SUBOBJECT* AddSubobject(const D3D12_RAYTRACING_PIPELINE_CONFIG1& subobjectDesc)
    {
        return AddSubobject(&subobjectDesc, sizeof(subobjectDesc), D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG);
    }

    const D3D12_STATE_SUBOBJECT* AddSubobject(const D3D12_HIT_GROUP_DESC& subobjectDesc)
    {
        return AddSubobject(&subobjectDesc, sizeof(subobjectDesc), D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP);
    }
};

struct ShaderIdentifier
{
    u8 Data[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES]{};

    ShaderIdentifier() = default;
    explicit ShaderIdentifier(const void* pIdentifier)
    {
        memcpy(Data, pIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    }
};
}