#pragma once
#include "../D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::content::texture {

void Shutdown();
id_t AddTexture(const u8* const blob);
void RemoveTexture(id_t id);
void GetDescriptorIndices(const id_t* const textureIDs, u32 idCount, u32* const outIndices);
[[nodiscard]] DescriptorHandle GetDescriptorHandle(id_t textureID, u32 mipLevel = 0, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
[[nodiscard]] DescriptorHandle GetDescriptorHandle(id_t textureID, u32 arrayIndex, u32 mipLevel, u32 depthIndex, DXGI_FORMAT format, bool isCubemap = false);
[[nodiscard]] const DXResource* const GetResource(id_t textureID);

[[nodiscard]] id_t AddIcon(const u8* const blob);


//TODO: make this universal
struct GeneratedTextureData
{
	constexpr static u32 MAX_MIPS{ 1 };
	u32 Width;
	u32 Height;
	u32 ArraySize;
	u32 MipLevels;
	DXGI_FORMAT Format;
	u8 Stride;
	u32 Flags;
	u32 SubresourceSize{ 0 };
	u8* SubresourceData{ nullptr };
};
[[nodiscard]] id_t CreateTextureFromGeneratedData(const GeneratedTextureData& data);
}