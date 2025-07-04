#include "D3D12Texture.h"
#include "Utilities/IOStream.h"
#include "../D3D12Upload.h"
#include "Content/ResourceCreation.h"

namespace mofu::graphics::d3d12::content::texture {
namespace {

util::FreeList<D3D12Texture> textures{};
util::FreeList<u32> descriptorIndices{};
std::mutex textureMutex{};

D3D12Texture 
CreateTextureFromResourceData(const u8* const blob)
{
    assert(blob);
    util::BlobStreamReader reader{ blob };
    const u32 width{ reader.Read<u32>() };
    const u32 height{ reader.Read<u32>() };
    u32 depth{ 1 };
    u32 arraySize{ reader.Read<u32>() };
    const u32 flags{ reader.Read<u32>() };
    const u32 mipLevels{ reader.Read<u32>() };
    const DXGI_FORMAT format{ (DXGI_FORMAT)reader.Read<u32>() };
    const bool is3d{ (flags & mofu::content::TextureFlags::IsVolumeMap) != 0 };
    assert(mipLevels < D3D12Texture::MAX_MIPS);

    u32 depthPerMipLevel[D3D12Texture::MAX_MIPS];
    for (u32 i{ 0 }; i < D3D12Texture::MAX_MIPS; ++i)
    {
        depthPerMipLevel[i] = 1;
    }

    if (is3d)
    {
        depth = arraySize;
        arraySize = 1;
        u32 depthPerMip{ depth };
        for (u32 i{ 0 }; i < mipLevels; ++i)
        {
            depthPerMipLevel[i] = depthPerMip;
            depthPerMip = std::max(depthPerMip >> 1, (u32)1);
        }
    }

    Vec<D3D12_SUBRESOURCE_DATA> subresources{};
    for (u32 i{ 0 }; i < arraySize; ++i)
    {
        for (u32 j{ 0 }; j < mipLevels; ++j)
        {
            const u32 rowPitch{ reader.Read<u32>() };
            const u32 slicePitch{ reader.Read<u32>() };

            subresources.emplace_back(D3D12_SUBRESOURCE_DATA{ reader.Position(), rowPitch, slicePitch });
            // skip the rest of the slices
            //TODO: reader.Skip(slicePitch * depthPerMipLevel[j]);
            reader.Skip(slicePitch);
        }
    }

    D3D12_RESOURCE_DESC1 desc{};
    //TODO: handle 1D
    desc.Dimension = is3d ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = is3d ? (u16)depth : (u16)arraySize;
    desc.MipLevels = (u16)mipLevels;
    desc.Format = format;
    desc.SampleDesc = { 1,0 };
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    assert(!(flags & mofu::content::TextureFlags::IsCubeMap && (arraySize % 6 != 0)));
    const u32 subresourceCount{ arraySize * mipLevels };
    assert(subresourceCount != 0);

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT* const layouts{ 
        (D3D12_PLACED_SUBRESOURCE_FOOTPRINT* const)_alloca(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) * subresourceCount) };
    u32* const numRows{ (u32* const)_alloca(sizeof(u32) * subresourceCount) };
    u64* const rowSizes{ (u64* const)_alloca(sizeof(u64) * subresourceCount) };
    u64 requiredSize{ 0 };
    DXDevice* device{ core::Device() };

    device->GetCopyableFootprints1(&desc, 0, subresourceCount, 0, layouts, numRows, rowSizes, &requiredSize);
    assert(requiredSize && requiredSize < UINT_MAX);

    upload::D3D12UploadContext context{ (u32)requiredSize };
    u8* const cpuAddress{ (u8* const)context.CpuAddress() };
    for (u32 subresourceIdx{ 0 }; subresourceIdx < subresourceCount; ++subresourceIdx)
    {
        const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout{ layouts[subresourceIdx] };
        const u32 subresourceHeight{ numRows[subresourceIdx] };
        const u32 subresourceDepth{ layout.Footprint.Depth };
        const D3D12_SUBRESOURCE_DATA& subresource{ subresources[subresourceIdx] };

        const D3D12_MEMCPY_DEST copyDest{cpuAddress + layout.Offset, layout.Footprint.RowPitch, layout.Footprint.RowPitch * subresourceHeight};

        for (u32 depthIdx{ 0 }; depthIdx < subresourceDepth; ++depthIdx)
        {
            u8* const srcSlice{(u8* const)subresource.pData + subresource.SlicePitch * depthIdx };
            u8* const destSlice{ (u8* const)copyDest.pData + copyDest.SlicePitch * depthIdx };

            for (u32 rowIdx{ 0 }; rowIdx < subresourceHeight; ++rowIdx)
            {
                memcpy(destSlice + copyDest.RowPitch * rowIdx, srcSlice + subresource.RowPitch * rowIdx, rowSizes[subresourceIdx]);
            }
        }
    }

    DXResource* resource{ nullptr };
    DXCall(device->CreateCommittedResource2(&d3dx::HeapProperties.DEFAULT_HEAP, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, nullptr, IID_PPV_ARGS(&resource)));

    DXResource* uploadBuffer{ context.UploadBuffer() };
    for (u32 i{ 0 }; i < subresourceCount; ++i)
    {
        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = uploadBuffer;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layouts[i];

        D3D12_TEXTURE_COPY_LOCATION dest{};
        dest.pResource = resource;
        dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dest.SubresourceIndex = i;

        context.CommandList()->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);
    }
    context.EndUpload();

    assert(resource);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    D3D12TextureInitInfo info{};
    info.resource = resource;

    if (flags & mofu::content::TextureFlags::IsCubeMap)
    {
        srv_desc.Format = format;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        if (arraySize > 6)
        {
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
            srv_desc.TextureCubeArray.MostDetailedMip = 0;
            srv_desc.TextureCubeArray.MipLevels = mipLevels;
            srv_desc.TextureCubeArray.NumCubes = arraySize / 6;
        }
        else
        {
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srv_desc.TextureCube.MostDetailedMip = 0;
            srv_desc.TextureCube.MipLevels = mipLevels;
            srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;
        }

        info.srvDesc = &srv_desc;
    }

    return D3D12Texture{ info };
}

} // anonymous namespace

// NOTE: expects data to contain:
// struct {
//     u32 width, height, arraySize (or depth), flags, mipLevels, format,
//     struct {
//         u32 rowPitch, slicePitch,
//         u8 image[mip_level][slicePitch * depthPerMip]
//     } images[]
// } texture;
id_t 
AddTexture(const u8* const blob)
{
    assert(blob);
    D3D12Texture texture{ CreateTextureFromResourceData(blob) };
    std::lock_guard lock{ textureMutex };
    const id_t id{ textures.add(std::move(texture)) };
    descriptorIndices.add(textures[id].Srv().index);
    return id;
}

void 
RemoveTexture(id_t id)
{
    std::lock_guard lock{ textureMutex };
    textures.remove(id);
    descriptorIndices.remove(id);
}

void 
GetDescriptorIndices(const id_t* const textureIDs, u32 idCount, u32* const outIndices)
{
    assert(textureIDs && idCount && outIndices);
    std::lock_guard lock{ textureMutex };
    for (u32 i{ 0 }; i < idCount; ++i)
    {
        assert(id::IsValid(textureIDs[i]));
        outIndices[i] = descriptorIndices[textureIDs[i]];
    }
}

const DescriptorHandle&
GetDescriptorHandle(id_t textureID)
{
    assert(id::IsValid(textureID));
    std::lock_guard lock{ textureMutex };
    return textures[textureID].Srv();
}

}