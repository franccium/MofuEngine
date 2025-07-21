#include "D3D12Texture.h"
#include "Utilities/IOStream.h"
#include "../D3D12Upload.h"
#include "Content/ResourceCreation.h"

namespace mofu::graphics::d3d12::content::texture {
namespace {

util::FreeList<D3D12Texture> textures{};
util::FreeList<D3D12Texture> icons{};
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
            for (u32 k{ 0 }; k < depthPerMipLevel[j]; ++k)
            {
                const u32 rowPitch{ reader.Read<u32>() };
                const u32 slicePitch{ reader.Read<u32>() };
                subresources.emplace_back(D3D12_SUBRESOURCE_DATA{ reader.Position(), rowPitch, slicePitch });
                // skip the rest of the slices
                reader.Skip(slicePitch * depthPerMipLevel[j]);
            }
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
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    D3D12TextureInitInfo info{};
    info.resource = resource;

    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if (flags & mofu::content::TextureFlags::IsCubeMap)
    {
        if (arraySize > 6)
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
            srvDesc.TextureCubeArray.MostDetailedMip = 0;
            srvDesc.TextureCubeArray.MipLevels = mipLevels;
            srvDesc.TextureCubeArray.NumCubes = arraySize / 6;
        }
        else
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.TextureCube.MostDetailedMip = 0;
            srvDesc.TextureCube.MipLevels = mipLevels;
            srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        }
    }
    else if (flags & mofu::content::TextureFlags::IsVolumeMap)
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        srvDesc.Texture3D.MipLevels = mipLevels;
        srvDesc.Texture3D.MostDetailedMip = 0;
        srvDesc.Texture3D.ResourceMinLODClamp = 0.f;
    }
    else
    {
        if (arraySize > 1)
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.ArraySize = arraySize;
            srvDesc.Texture2DArray.FirstArraySlice = 0;
            srvDesc.Texture2DArray.MipLevels = mipLevels;
            srvDesc.Texture2DArray.MostDetailedMip = 0;
            srvDesc.Texture2DArray.PlaneSlice = 0;
            srvDesc.Texture2DArray.ResourceMinLODClamp = 0.f;
        }
        else
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = mipLevels;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.PlaneSlice = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.f;
        }
    }
    info.srvDesc = &srvDesc;

    return D3D12Texture{ info };
}

D3D12Texture
CreateIcon(const u8* const blob)
{
    assert(blob);
    util::BlobStreamReader reader{ blob };
    const u32 width{ reader.Read<u32>() };
    const u32 height{ reader.Read<u32>() };
    const DXGI_FORMAT format{ (DXGI_FORMAT)reader.Read<u32>() };

    const u32 rowPitch{ reader.Read<u32>() };
    const u32 slicePitch{ reader.Read<u32>() };
    D3D12_SUBRESOURCE_DATA subresourceData{ reader.Position(), rowPitch, slicePitch };

    D3D12_RESOURCE_DESC1 desc{};
    //TODO: handle 1D
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc = { 1,0 };
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    const u32 subresourceCount{ 1 };
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT* const layouts{
        (D3D12_PLACED_SUBRESOURCE_FOOTPRINT* const)_alloca(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) * 1) };
    u32 numRows{ 0 };
    u64 rowSizes{ 0 };
    u64 requiredSize{ 0 };
    DXDevice* device{ core::Device() };

    device->GetCopyableFootprints1(&desc, 0, 1, 0, layouts, &numRows, &rowSizes, &requiredSize);
    assert(requiredSize && requiredSize < UINT_MAX);

    upload::D3D12UploadContext context{ (u32)requiredSize };
    u8* const cpuAddress{ (u8* const)context.CpuAddress() };
    for (u32 subresourceIdx{ 0 }; subresourceIdx < 1; ++subresourceIdx)
    {
        const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout{ layouts[subresourceIdx] };
        const u32 subresourceHeight{ numRows };
        const u32 subresourceDepth{ layout.Footprint.Depth };

        const D3D12_MEMCPY_DEST copyDest{ cpuAddress + layout.Offset, layout.Footprint.RowPitch, layout.Footprint.RowPitch * subresourceHeight };

        for (u32 depthIdx{ 0 }; depthIdx < subresourceDepth; ++depthIdx)
        {
            u8* const srcSlice{ (u8* const)subresourceData.pData + subresourceData.SlicePitch * depthIdx };
            u8* const destSlice{ (u8* const)copyDest.pData + copyDest.SlicePitch * depthIdx };

            for (u32 rowIdx{ 0 }; rowIdx < subresourceHeight; ++rowIdx)
            {
                memcpy(destSlice + copyDest.RowPitch * rowIdx, srcSlice + subresourceData.RowPitch * rowIdx, rowSizes);
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
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    D3D12TextureInitInfo info{};
    info.resource = resource;

    //if (flags & mofu::content::TextureFlags::IsCubeMap)
    //{
    //    srvDesc.Format = format;
    //    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    //    if (arraySize > 6)
    //    {
    //        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
    //        srvDesc.TextureCubeArray.MostDetailedMip = 0;
    //        srvDesc.TextureCubeArray.MipLevels = mipLevels;
    //        srvDesc.TextureCubeArray.NumCubes = arraySize / 6;
    //    }
    //    else
    //    {
    //        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    //        srvDesc.TextureCube.MostDetailedMip = 0;
    //        srvDesc.TextureCube.MipLevels = mipLevels;
    //        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    //    }

    //    info.srvDesc = &srvDesc;
    //}

    return D3D12Texture{ info };
}

[[nodiscard]] DescriptorHandle
CreateSRVForMipLevel(DXResource* texture, u32 mipLevel, DXGI_FORMAT format)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = mipLevel;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    DescriptorHandle srvHandle = core::SrvHeap().AllocateDescriptor();
    core::Device()->CreateShaderResourceView(texture, &srvDesc, srvHandle.cpu);

    return srvHandle;
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

id_t
AddIcon(const u8* const blob)
{
    assert(blob);
    D3D12Texture iconTexture{ CreateIcon(blob) };
    //TODO: a seperate thing?
    std::lock_guard lock{ textureMutex };
    const id_t id{ textures.add(std::move(iconTexture)) };
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
    //assert(textureIDs && idCount && outIndices);
    std::lock_guard lock{ textureMutex };
    for (u32 i{ 0 }; i < idCount; ++i)
    {
        assert(id::IsValid(textureIDs[i]));
        outIndices[i] = descriptorIndices[textureIDs[i]];
    }
}

DescriptorHandle
GetDescriptorHandle(id_t textureID, u32 mipLevel, DXGI_FORMAT format)
{
    assert(id::IsValid(textureID));
    std::lock_guard lock{ textureMutex };
    return mipLevel == 0 ? textures[textureID].Srv() : CreateSRVForMipLevel(textures[textureID].Resource(), mipLevel, format);
}

DescriptorHandle
GetDescriptorHandle(id_t textureID, u32 arrayIndex, u32 mipLevel, u32 depthIndex, DXGI_FORMAT format, bool isCubemap)
{
    assert(id::IsValid(textureID));
    std::lock_guard lock{ textureMutex };
    return textures[textureID].GetSRV(arrayIndex, mipLevel, depthIndex, format, isCubemap);
}

}