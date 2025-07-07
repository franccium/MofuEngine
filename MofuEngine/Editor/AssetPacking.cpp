#include "AssetPacking.h"
#include "Utilities/IOStream.h"
#include <filesystem>
#include <fstream>

namespace mofu::content {
namespace {
u64
GetTextureEditorPackedSize(texture::TextureData& data)
{
	constexpr u64 su32{ (sizeof(u32)) };
	u64 size{ 0 };

	// TextureImportSettings
	size += (u64)data.ImportSettings.Files.length();
	size += su32 * 5 + sizeof(f32) + sizeof(u8) * 4;
	
	size += su32 * 7;

	size += data.SubresourceSize;

	for (u32 i{ 0 }; i < data.Info.ArraySize; ++i)
	{
		for (u32 j{ 0 }; j < data.Info.MipLevels; ++j)
		{
		}
	}

	size += data.SubresourceSize;

	return size;
}

u64 
GetTextureEnginePackedSize(texture::TextureData& data)
{
	constexpr u64 su32{ (sizeof(u32)) };
	//TODO: fix this
	u64 size{ su32 + su32 + su32 + su32 + su32 };
    size += su32 + data.SubresourceSize;
    return size;
}

} // anonymous namespace

// NOTE: editor expects data to contain:
// struct {
//		TextureImportSettings
//		u32 width, height, arraySize (or depth), flags, mipLevels, format,
//		IBLPairGUID,
//		imagesDataSize,
//		imagesData
// } texture;
void
PackTextureForEditor(texture::TextureData& data, std::string_view filename)
{
	const u64 textureDataSize{ GetTextureEditorPackedSize(data) };
	u8* buffer{ new u8[textureDataSize] };
	u32 bufferSize{ (u32)textureDataSize };
	const texture::TextureInfo& info{ data.Info };

	util::BlobStreamWriter writer{ buffer, bufferSize };

	const texture::TextureImportSettings& importSettings{ data.ImportSettings };
	writer.WriteStringWithLength(importSettings.Files);
	writer.Write<u32>(importSettings.FileCount);
	writer.Write<u32>((u32)importSettings.Dimension);
	writer.Write<u32>(importSettings.MipLevels);
	writer.Write<f32>(importSettings.AlphaThreshold);
	writer.Write<u32>((u32)importSettings.OutputFormat);
	writer.Write<u32>(importSettings.CubemapSize);
	writer.Write<u8>(importSettings.PreferBC7);
	writer.Write<u8>(importSettings.Compress);
	writer.Write<u8>(importSettings.PrefilterCubemap);
	writer.Write<u8>(importSettings.MirrorCubemap);

	writer.Write<u32>(info.Width);
	writer.Write<u32>(info.Height);
	writer.Write<u32>(info.ArraySize);
	writer.Write<u32>(info.Flags);
	writer.Write<u32>(info.MipLevels);
	writer.Write<u32>(info.Format);
	writer.Write<u32>(id::INVALID_ID); //TODO: IBLPairGUID

	util::BlobStreamReader reader{ data.SubresourceData };
	for (u32 i{ 0 }; i < info.ArraySize; ++i)
	{
		for (u32 j{ 0 }; j < info.MipLevels; ++j)
		{
			u32 width{ reader.Read<u32>() };
			u32 height{ reader.Read<u32>() };
			u32 rowPitch{ reader.Read<u32>() };
			u32 slicePitch{ reader.Read<u32>() };
			writer.Write<u32>(width);
			writer.Write<u32>(height);
			writer.Write<u32>(rowPitch);
			writer.Write<u32>(slicePitch);
			// image.pixels
			writer.WriteBytes(reader.Position(), slicePitch);
			reader.Skip(slicePitch);
			// skip the rest of the slices
			//TODO: writer.WriteBytes(, slicePitch * depthPerMipLevel[j]);
		}
	}

	//TODO: refactor
	std::filesystem::path modelPath{ "Assets/Generated/"};
	modelPath.append(filename.data());
	modelPath.replace_extension(".etex");
	std::ofstream file{ modelPath, std::ios::out | std::ios::binary };
	if (!file) return;

	file.write(reinterpret_cast<const char*>(buffer), bufferSize);
	file.close();
}

// NOTE: engine expects data to contain:
// struct {
//     u32 width, height, arraySize (or depth), flags, mipLevels, format,
//     struct {
//         u32 rowPitch, slicePitch,
//         u8 image[mip_level][slicePitch * depthPerMip]
//     } images[]
// } texture;

// SubresourceData is:
/*
	for (u32 i{ 0 }; i < imageCount; ++i)
	{
		const Image& image{ images[i] };
		blob.Write((u32)image.width);
		blob.Write((u32)image.height);
		blob.Write((u32)image.rowPitch);
		blob.Write((u32)image.slicePitch);
		blob.WriteBytes(image.pixels, image.slicePitch);
	}
*/
void
PackTextureForEngine(texture::TextureData& data, std::string_view filename)
{
	const u64 textureDataSize{ GetTextureEnginePackedSize(data) };
	u8* buffer{ new u8[textureDataSize] };
	u32 bufferSize{ (u32)textureDataSize };

	util::BlobStreamWriter writer{ buffer, textureDataSize };
	const texture::TextureInfo& info{ data.Info };
	writer.Write<u32>(info.Width);
	writer.Write<u32>(info.Height);
	writer.Write<u32>(info.ArraySize);
	writer.Write<u32>(info.Flags);
	writer.Write<u32>(info.MipLevels);
	writer.Write<u32>(info.Format);

	util::BlobStreamReader reader{ data.SubresourceData };
	for (u32 i{ 0 }; i < info.ArraySize; ++i)
    {
        for (u32 j{ 0 }; j < info.MipLevels ; ++j)
        {
			reader.Skip(sizeof(u32) * 2); // skip Width and Height
			u32 rowPitch{ reader.Read<u32>() };
            writer.Write<u32>(rowPitch);
            u32 slicePitch{ reader.Read<u32>() };
            writer.Write<u32>(slicePitch);

			// image.pixels
            writer.WriteBytes(reader.Position(), slicePitch);
			reader.Skip(slicePitch);
            // skip the rest of the slices
            //TODO: writer.WriteBytes(, slicePitch * depthPerMipLevel[j]);
        }
    }

	//TODO:assert(writer.Offset() == textureDataSize);
	//TODO: refactor
	std::filesystem::path modelPath{ "Assets/Generated/"};
	modelPath.append(filename.data());
	modelPath.replace_extension(".tex");
	std::ofstream file{ modelPath, std::ios::out | std::ios::binary };
	if (!file) return;

	file.write(reinterpret_cast<const char*>(buffer), bufferSize);
	file.close();
}

}