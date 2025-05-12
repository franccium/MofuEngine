#pragma once
#include "CommonHeaders.h"
#include "Utilities/IOStream.h"

namespace mofu::content {

typedef struct CompiledShader
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
} const* CompiledShaderPtr;

bool LoadEngineShaders(std::unique_ptr<u8[]>& shaders, u64& size);

}