#pragma once
#include "CommonHeaders.h"
#include "ContentManagement.h"

namespace mofu::content {

struct CompiledShader
{
	static constexpr u32 HASH_LENGTH{ 16 };

	constexpr u64 BytecodeSize() const { return _bytecodeSize; }
	constexpr const u8* const Hash() const { return &_hash[0]; }
	constexpr const u8* const Bytecode() const { return &_bytecode; }
	constexpr const u64 GetBufferSize() const { return sizeof(_bytecodeSize) + HASH_LENGTH + _bytecodeSize; }
	constexpr static u64 GetRequiredBufferSize(u64 size) { return sizeof(_bytecodeSize) + HASH_LENGTH + size; }

private:
	u64 _bytecodeSize;
	u8 _hash[HASH_LENGTH];
	u8 _bytecode;
};
using CompiledShaderPtr = const CompiledShader*;

struct LodOffset
{
	u16 Offset;
	u16 Count;
};

struct UploadedGeometryInfo
{
	id_t GeometryContentID;
	u32 SubmeshCount;
	Vec<id_t> SubmeshGpuIDs;
};

id_t CreateResourceFromBlob(const void* const blob, AssetType::type resourceType);
void DestroyResource(id_t resourceId, AssetType::type resourceType);

void GetSubmeshGpuIDs(id_t geometryContentID, u32 idCount, id_t* const outGpuIDs, u32 counter);
void GetLODOffsets(const id_t* const geometryIDs, const f32* const thresholds, u32 idCount, Vec<LodOffset>& offsets);

id_t AddShaderGroup(const u8* const* shaders, u32 shaderCount, const u32* const keys);
void RemoveShaderGroup(id_t id);
CompiledShaderPtr GetShader(id_t groupID, u32 shaderKey);

//TODO: bad idea 
UploadedGeometryInfo GetLastUploadedGeometryInfo();
}