#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::upload {
class D3D12UploadContext
{
public:
	DISABLE_COPY_AND_MOVE(D3D12UploadContext);
	D3D12UploadContext(u32 alignedSize);
	~D3D12UploadContext() { assert(_frameIndex == U32_INVALID_ID); }

	void EndUpload();

	[[nodiscard]] constexpr DXGraphicsCommandList* const CommandList() const { return _cmdList; }
	[[nodiscard]] constexpr DXResource* const UploadBuffer() const { return _uploadBuffer; }
	[[nodiscard]] constexpr void* const CpuAddress() const { return _cpuAddress; }
			
private:
	DEBUG_OP(D3D12UploadContext() = default); // so we can mark the instances as invalid after an upload

	DXGraphicsCommandList* _cmdList{ nullptr };
	u32 _frameIndex{ U32_INVALID_ID };
	DXResource* _uploadBuffer{ nullptr };
	void* _cpuAddress{ nullptr };
};

bool Initialize();
void Shutdown();
}